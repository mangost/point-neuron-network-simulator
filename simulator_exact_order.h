#ifndef HEADER_SIMULATOR_EXACT_ORDER
#define HEADER_SIMULATOR_EXACT_ORDER

#include "common_header.h"
#include "poisson_generator.h"
#include "neuron_system_utils.h"
#include "neuron_population.h"
#include "simulator_base.h"

// A simple simulator for networks without in-dt spike order correction.
class NeuronSimulatorSimple :public NeuronSimulatorBase
{
public:
  TyPoissonTimeVec poisson_time_vec;
  double t, dt;

  NeuronSimulatorSimple(const TyNeuronalParams &pm, double _dt, double t0)
  {
    dt = _dt;
    t = t0;
    poisson_time_vec.Init(pm.arr_pr, pm.arr_ps, pm.arr_pri, pm.arr_psi, t);
  }
  
  double GetT() const override
  {
    return t;
  }

protected:
  // Evolve all neurons without synaptic interaction
  // TODO: consider move this function to population class
  MACRO_NO_INLINE void NextStepNoInteract(NeuronPopulationBase * p_neu_pop,
      TySpikeEventVec &spike_events, double dt)
  {
    double t_step_end = t + dt;
    // Loop over neurons
    for (int j = 0; j < p_neu_pop->n_neurons(); j++) {
      TyPoissonTimeSeq &poisson_time_seq = poisson_time_vec[j];
      double t_local = t;
      // Loop over Poisson events in this dt
      while (poisson_time_seq.Front().time < t_step_end) {
        p_neu_pop->NoInteractDt(j, poisson_time_seq.Front().time - t_local, t_local, spike_events);
        t_local = poisson_time_seq.Front().time;
        p_neu_pop->InjectDeltaInput(j, poisson_time_seq.Front().strength);
        poisson_time_seq.PopAndFill();  // Next event
      }
      p_neu_pop->NoInteractDt(j, t_step_end - t_local, t_local, spike_events);
    }
  }

public:
  void NextDt(NeuronPopulationBase * p_neu_pop,
      TySpikeEventVec &ras, std::vector<size_t> &vec_n_spike) override
  {
    TySpikeEventVec spike_events;        // Spike events in this dt
    poisson_time_vec.SaveIdxAndClean();  // Clean Poisson queue

    NextStepNoInteract(p_neu_pop, spike_events, dt);

    std::sort(spike_events.begin(), spike_events.end());
    for (auto it = spike_events.begin(); it != spike_events.end(); it++) {
      p_neu_pop->SynapticInteraction(*it);
      vec_n_spike[it->id]++;
    }
    ras.insert(ras.end(), spike_events.begin(), spike_events.end());
    t += dt;
  }

  TyPoissonTimeVec & Get_poisson_time_vec() override
  {
    return poisson_time_vec;
  }
  const TyPoissonTimeVec & Get_poisson_time_vec() const override
  {
    return poisson_time_vec;
  }
};


/**
  Solver for pulse-coupled neuron model:
    After a test step (without synaptic interaction), evolve the system
    to the time of the first spike in this delta-t step, perform the synaptic
    interaction. Then start another test step for the remaining interval,
    recursively until there is no spike in the remaining interval.
  Based on the interface NeuronPopulationBase which describes the population dynamic.

  Time cost:
    # of calls to NextStepSingleNeuronQuiet(): (1/dt + pr + 2*p*fr)*p*T
    # of synaptic interaction: (# of edges)*fr*T.
  where the fr is mean firing rate over all neurons, p=nE+nI.
*/
class NeuronSimulatorExactSpikeOrder :public NeuronSimulatorSimple
{
public:
  NeuronSimulatorExactSpikeOrder(const TyNeuronalParams &pm, double _dt, double t0)
    :NeuronSimulatorSimple(pm, _dt, t0)
  {}

public:
  // Evolve the whole system one dt, with interaction
  void NextDt(NeuronPopulationBase * p_neu_pop,
      TySpikeEventVec &ras, std::vector< size_t > &vec_n_spike) override
  {
    double t_end = t + dt;
    struct TyNeuronalDymState bk_neu_state;
    struct TySpikeEvent heading_spike_event(qNaN, -1);
    TySpikeEventVec spike_events;

    dbg_printf("===== NextDt(): t = %f .. %f\n", t, t_end);
    poisson_time_vec.SaveIdxAndClean();
    while (true) {
      spike_events.clear();
      bk_neu_state = p_neu_pop->GetDymState();
      NextStepNoInteract(p_neu_pop, spike_events, t_end - t);  // Try evolve till the step end
      if (spike_events.empty()) {
        t = t_end;
        break;
      } else {
        // Find out the first spike.
        heading_spike_event = *std::min_element(spike_events.begin(), spike_events.end());
        dbg_printf("NextDt(): Restart calculation from t = %.17g up to %.17g\n",
                   t, heading_spike_event.time);
        dbg_printf("          For dealing neuron [%d] spike at t = %.17g\n",
                   heading_spike_event.id, heading_spike_event.time);
        // Really evolve the whole system.
        poisson_time_vec.RestoreIdx();          // replay the poisson events
        spike_events.clear();
        *p_neu_pop = bk_neu_state;
        NextStepNoInteract(p_neu_pop, spike_events, heading_spike_event.time - t);
        // Record all spike events during the time step above.
        // Ideally, there should be only one event: heading_spike_event.
        bool b_heading_spike_pushed = false;
        for (size_t i = 0; i < spike_events.size(); i++) {
          if (spike_events[i].id == heading_spike_event.id) {
            b_heading_spike_pushed = true;
          } else {
            fprintf(stderr, "Unexpected spike before \"first\" spike: [%lu] id = %d, t = %.17g - %.17g\n",
                i, spike_events[i].id, heading_spike_event.time,
                heading_spike_event.time - spike_events[i].time);
          }
          // Here does not do `ras.emplace_back(spike_events[i])' because
          // effectively the spike interaction are done at the same time.
          ras.emplace_back(heading_spike_event.time, spike_events[i].id);
          vec_n_spike[spike_events[i].id]++;
          p_neu_pop->SynapticInteraction(spike_events[i]);
        }
        // Force the neuron to spike, if not already.
        if (!b_heading_spike_pushed) {
          p_neu_pop->ForceReset(heading_spike_event.id);
          ras.emplace_back(heading_spike_event);
          vec_n_spike[heading_spike_event.id]++;
          p_neu_pop->SynapticInteraction(heading_spike_event);
        }
        t = heading_spike_event.time;
        poisson_time_vec.SaveIdxAndClean();
      }
    }
  }

};

/*
  The algorithm is (almost) identical to NeuronSimulatorExactSpikeOrder.
  (if neuron_model.NextStepSingleNeuronQuiet() solves the neuron exactly,
   then they produce exactly the same results)
  Should be much faster than NeuronSimulatorExactSpikeOrder when the
  network is sparse.

  Time cost:
    # of calls to NextStepSingleNeuronQuiet(): (1/dt + pr + 2*p*sp*fr)*p*T
    # of synaptic interaction: (# of edges)*fr*T.
  where `fr' is mean firing rate over all neurons, p=nE+nI,
        `sp' is mean number of out edges for each neuron.
*/
class NeuronSimulatorExactSpikeOrderSparse
: public NeuronSimulatorSimple
{
public:

  NeuronSimulatorExactSpikeOrderSparse(const TyNeuronalParams &_pm, double _dt, double t0)
    :NeuronSimulatorSimple(_pm, _dt, t0)
  {
  }

protected:
  // Evolve all neurons without synaptic interaction
  MACRO_NO_INLINE void NextStepNoInteractToTime(
      NeuronPopulationBase * p_neu_pop,
      const TyArrVals &bk_state_time,
      const std::vector<int> &ids_affected,
      TySpikeEventVec &spike_events,
      double t_step_end)
  {
    for (size_t jj = 0; jj < ids_affected.size(); jj++) {
      const int j = ids_affected[jj];
      dbg_printf("----- NextStepNoInteractToTime(), Neuron %d, t = %f .. %f\n", j, bk_state_time[j], t_step_end);
      //! tmp_neu_state.dym_vals must be Row major !
      TyPoissonTimeSeq &poisson_time_seq = poisson_time_vec[j];
      double t_local = bk_state_time[j];
      while (poisson_time_seq.Front().time < t_step_end) {
        dbg_printf("  receive %lu-th (%lu) Poisson input at %f\n",
                   poisson_time_seq.id_seq, poisson_time_seq.size(),
                   poisson_time_seq.Front().time);
        dbg_printf("  time from %f to %f\n", t_local, poisson_time_seq.Front().time);
        p_neu_pop->NoInteractDt(j,
            poisson_time_seq.Front().time - t_local, t_local, spike_events);
        t_local = poisson_time_seq.Front().time;
        p_neu_pop->InjectDeltaInput(j, poisson_time_seq.Front().strength);
        poisson_time_seq.PopAndFill();
      }
      dbg_printf("  time from %f to %f\n", t_local, t_step_end);
      p_neu_pop->NoInteractDt(j,
          t_step_end - t_local, t_local, spike_events);
      /*dbg_printf("  t=%f end state: %f, %f, %f\n", t_step_end, dym_val[0], dym_val[1], dym_val[2]);*/
    }
  }

public:

  // Evolve the whole system one dt, with interaction.
  // Save spike events in this `dt' in `ras',
  // and add spike count to `vec_n_spike'.
  // This version also roll back the firing neuron

  std::vector<int> ids_affected;             // index of affected neurons
  std::vector<bool> bool_affected;           // hash for `ids_affected'

  // `bk_neu_state' is state at time `bk_state_time'
  TyArrVals bk_state_time;
  struct TyNeuronalDymState bk_neu_state;

  void NextDt(NeuronPopulationBase * p_neu_pop,
      TySpikeEventVec &ras, std::vector< size_t > &vec_n_spike)
  {
    double t_end = t + dt;

    struct TySpikeEvent heading_spike_event(qNaN, -1);
    ids_affected.clear();
    bool_affected.clear();
    for (int i = 0; i < p_neu_pop->n_neurons(); i++) {
      bool_affected.push_back(false);
    }

    bk_state_time.resize(p_neu_pop->n_neurons());

    // `spike_events' holds spike events between `bk_state_time' and `t_end'
    TySpikeEventVec spike_events;
    // for fixed step, should contain no event
    TySpikeEventVec tmp_spike_events;

    // Save states at time `bk_state_time', then do a test step.
    dbg_printf("===== NextDt(): t = %f .. %f\n", t, t_end);
    for (double &tp : bk_state_time) tp = t;
    bk_neu_state = p_neu_pop->GetDymState();
    poisson_time_vec.SaveIdxAndClean();
    NextStepNoInteract(p_neu_pop, spike_events, t_end - t);

    // Evolve along the accurate order of spikes.
    // Each loop deals with one spike.
    while (!spike_events.empty()) {
      // Find out the first spike.
      auto heading_it = std::min_element(spike_events.begin(), spike_events.end());
      heading_spike_event = *heading_it;
      spike_events.erase(heading_it);     // "pop" the event
      dbg_printf("Dealing neuron %d spike at t = %f. Spikes left: %lu\n",
                 heading_spike_event.id, heading_spike_event.time, spike_events.size());

      // Find the index of affected neurons. Including the spiking one.
      for (const int &id : ids_affected)  // remove hash of last loop
        bool_affected[id] = false;
      ids_affected.clear();
      dbg_printf("  Affected neurons:");
      const auto &net = p_neu_pop->GetNeuronalParamsPtr()->net;
      for (SparseMat::InnerIterator it(net, heading_spike_event.id); it; ++it) {
        ids_affected.push_back(it.row());
        bool_affected[it.row()] = true;
        dbg_printf("  %d", it.row());
      }
      ids_affected.push_back(heading_spike_event.id);
      bool_affected[heading_spike_event.id] = true;
      dbg_printf(".\n");

      // Roll back the affected neurons.
      p_neu_pop->ScatterCopy(bk_neu_state, ids_affected);
      poisson_time_vec.RestoreIdx(ids_affected);
      spike_events.erase(
        remove_if(spike_events.begin(), spike_events.end(),
          [&](const TySpikeEvent &e) {
            return bool_affected[e.id] &&
              !(bk_state_time[e.id]==heading_spike_event.time
                && e.time==heading_spike_event.time);
          }),
        spike_events.end());

      // Evolve the affected neurons to the time of first spike. (fix step)
      tmp_spike_events.clear();
      NextStepNoInteractToTime(p_neu_pop, bk_state_time, ids_affected,
        tmp_spike_events, heading_spike_event.time);
      // deal with unexpected spikes
      bool b_heading_spike_pushed = false;
      for (size_t i = 0; i < tmp_spike_events.size(); i++) {
        if (tmp_spike_events[i].id == heading_spike_event.id) {
          b_heading_spike_pushed = true;
        } else {
          // possible extra spikes due to inaccurate computation
          cerr << "Unexpected spike (spike before \"first\" spike):  ["
            << i << "] id = " << tmp_spike_events[i].id
            << " time = " << tmp_spike_events[i].time << "\n";
          spike_events.emplace_back(heading_spike_event.time,
              tmp_spike_events[i].id);
        }
      }
      if (!b_heading_spike_pushed) {
        p_neu_pop->ForceReset(heading_spike_event.id);
      }

      // Perform synaptic interactions.
      p_neu_pop->SynapticInteraction(heading_spike_event);
      ras.emplace_back(heading_spike_event);
      vec_n_spike[heading_spike_event.id]++;

      // Save roll back base.
      for (const int &id : ids_affected)
        bk_state_time[id] = heading_spike_event.time;
      bk_neu_state.ScatterCopy(p_neu_pop->GetDymState(), ids_affected);
      poisson_time_vec.SaveIdxAndClean(ids_affected);

      // Evolve the affected neurons to t_end. (test step)
      tmp_spike_events.clear();
      NextStepNoInteractToTime(p_neu_pop, bk_state_time, ids_affected,
        spike_events, t_end);
    }
    t = t_end;
  }
};

class NeuronSimulatorExactSpikeOrderSparse2
: public NeuronSimulatorExactSpikeOrderSparse
{
public:

  NeuronSimulatorExactSpikeOrderSparse2(const TyNeuronalParams &_pm, double _dt, double t0)
    :NeuronSimulatorExactSpikeOrderSparse(_pm, _dt, t0)
  {
  }

  // Evolve the whole system one dt, with interaction.
  // Save spike events in this `dt' in `ras',
  // and add spike count to `vec_n_spike'.
  void NextDt(NeuronPopulationBase * p_neu_pop,
      TySpikeEventVec &ras, std::vector< size_t > &vec_n_spike)
  {
    double t_end = t + dt;

    struct TySpikeEvent heading_spike_event(qNaN, -1);
    std::vector<int> ids_affected;             // index of affected neurons
    std::vector<bool> bool_affected;           // hash for `ids_affected'
    std::vector<bool> bool_fired;  // assume one neuron can not fire twice in one dt
    for (int i = 0; i < p_neu_pop->n_neurons(); i++) {
      bool_affected.push_back(false);
      bool_fired.push_back(false);
    }

    // `bk_neu_state' is state at time `bk_state_time'
    TyArrVals bk_state_time(p_neu_pop->n_neurons());
    struct TyNeuronalDymState bk_neu_state;

    // `spike_events' holds spike events between `bk_state_time' and `t_end'
    TySpikeEventVec spike_events;
    // for fixed step, should contain no event
    TySpikeEventVec tmp_spike_events;

    // Save states at time `bk_state_time', then do a test step.
    dbg_printf("===== NextDt(): t = %f .. %f\n", t, t_end);
    for (double &tp : bk_state_time) tp = t;
    bk_neu_state = p_neu_pop->GetDymState();
    poisson_time_vec.SaveIdxAndClean();
    NextStepNoInteract(p_neu_pop, spike_events, t_end - t);

    // Evolve in the accurate order of spikes.
    // Each loop deals with one spike.
    while (!spike_events.empty()) {
      // Find out the first spike.
      auto heading_it = std::min_element(spike_events.begin(), spike_events.end());
      heading_spike_event = *heading_it;
      spike_events.erase(heading_it);     // "pop" the event
      bool_fired[heading_spike_event.id] = true;
      dbg_printf("Dealing neuron %d spike at t = %f. Spikes left: %lu\n",
                 heading_spike_event.id, heading_spike_event.time, spike_events.size());

      // Find the index of affected neurons. Not including the spiking one.
      for (const int &id : ids_affected)  // remove hash of last loop
        bool_affected[id] = false;
      ids_affected.clear();
      dbg_printf("  Affected neurons:");
      const auto &net = p_neu_pop->GetNeuronalParamsPtr()->net;
      for (SparseMat::InnerIterator it(net, heading_spike_event.id); it; ++it) {
        ids_affected.push_back(it.row());
        bool_affected[it.row()] = true;
        dbg_printf("  %d", it.row());
      }
      dbg_printf(".\n");

      // Roll back the affected neurons.
      p_neu_pop->ScatterCopy(bk_neu_state, ids_affected);
      poisson_time_vec.RestoreIdx(ids_affected);
      spike_events.erase(
        remove_if(spike_events.begin(), spike_events.end(),
          [&](const TySpikeEvent &e) { return bool_affected[e.id]; }),
        spike_events.end());

      // Evolve the affected neurons to the time of first spike. (fix step)
      tmp_spike_events.clear();
      NextStepNoInteractToTime(p_neu_pop, bk_state_time, ids_affected,
        tmp_spike_events, heading_spike_event.time);
      // deal with unexpected spikes
      for (size_t i = 0; i < tmp_spike_events.size(); i++) {
        if (bool_fired[tmp_spike_events[i].id])
          continue;
        // possible extra spikes due to inaccurate computation
        cerr << "Unexpected spike (spike before \"first\" spike):  ["
          << i << "] id = " << tmp_spike_events[i].id
          << " time = " << tmp_spike_events[i].time << "\n";
        spike_events.emplace_back(heading_spike_event.time,
            tmp_spike_events[i].id);
      }

      // Perform synaptic interactions.
      p_neu_pop->SynapticInteraction(heading_spike_event);
      ras.emplace_back(heading_spike_event);
      vec_n_spike[heading_spike_event.id]++;

      // Save roll back base.
      for (const int &id : ids_affected)
        bk_state_time[id] = heading_spike_event.time;
      bk_neu_state.ScatterCopy(p_neu_pop->GetDymState(), ids_affected);
      poisson_time_vec.SaveIdxAndClean(ids_affected);

      // Evolve the affected neurons to t_end. (test step)
      tmp_spike_events.clear();
      NextStepNoInteractToTime(p_neu_pop, bk_state_time, ids_affected,
        tmp_spike_events, t_end);
      for (size_t i = 0; i < tmp_spike_events.size(); i++) {
        if (bool_fired[tmp_spike_events[i].id])
          continue;
        spike_events.emplace_back(tmp_spike_events[i]);
      }
    }
    t = t_end;
  }
};

// A simple simulator for networks with delayed synaptics.
class NeuronSimulatorBigDelay :public NeuronSimulatorSimple
{
protected:
  // or try circular_buffer_space_optimized in boost/circular_buffer.hpp
  typedef std::deque<TySpikeEvent> TySpikeEventQue;
  std::vector< TySpikeEventQue > se_que_vec;  // synaptic events

  // Go one step without spike timing correction.
  MACRO_NO_INLINE void NextStepNoInteract(NeuronPopulationBase * p_neu_pop,
      TySpikeEventVec &spike_events, double dt)
  {
    double t_step_end = t + dt;
    
    std::vector<TySpikeEventStrength> se_list;   // id<0 poisson; id>=0 spike

    for (int j = 0; j < p_neu_pop->n_neurons(); j++) {
      se_list.clear();

      // All Poisson events for neuron j in this dt.
      TyPoissonTimeSeq &pe_seq = poisson_time_vec[j];
      while (pe_seq.Front().time < t_step_end) {
        se_list.emplace_back(pe_seq.Front().time, -1, pe_seq.Front().strength);
        pe_seq.PopAndFill();
      }
      
      // All Spike events for neuron j in this dt.
      TySpikeEventQue  &se_que = se_que_vec[j];
      while (se_que.size() != 0
            && se_que.front().time < t_step_end) {
        se_list.emplace_back(se_que.front().time, se_que.front().id, qNaN);
        se_que.pop_front();
      }

      std::sort(se_list.begin(), se_list.end());

      // Apply the events.
      double t_local = t;
      for (size_t i = 0; i < se_list.size(); i++) {
        p_neu_pop->NoInteractDt(j, se_list[i].time - t_local, t_local, spike_events);
        t_local = se_list[i].time;
        if (se_list[i].id >= 0) {
          p_neu_pop->SynapticInteraction(j, se_list[i].id);
        } else {
          p_neu_pop->InjectDeltaInput(j, se_list[i].strength);
        }
      }

      p_neu_pop->NoInteractDt(j, t_step_end - t_local, t_local, spike_events);
    }
  }

public:
  NeuronSimulatorBigDelay(const TyNeuronalParams &_pm, double _dt, double t0)
    :NeuronSimulatorSimple(_pm, _dt, t0)
  {
    se_que_vec.resize(_pm.n_total());
  }

  void NextDt(NeuronPopulationBase * p_neu_pop,
      TySpikeEventVec &ras, std::vector< size_t > &vec_n_spike) override
  {
    double synaptic_delay = p_neu_pop->SynapticDelay();
    if (synaptic_delay <= dt) {
      throw "Delay too small for this simulator";
    }
    TySpikeEventVec spike_events;
    poisson_time_vec.SaveIdxAndClean();

    // Go one step.
    NextStepNoInteract(p_neu_pop, spike_events, dt);

    for (auto const &se : spike_events) {
      vec_n_spike[se.id]++;  // Count spikes.
    }

    std::sort(spike_events.begin(), spike_events.end());
    ras.insert(ras.end(), spike_events.begin(), spike_events.end());

    // Insert new synaptic events.
    const auto &net = p_neu_pop->GetNeuronalParamsPtr()->net;
    for (auto const &se : spike_events) {
      // Loop over affected neurons.
      for (SparseMat::InnerIterator it(net, se.id); it; ++it) {
        se_que_vec[it.row()].emplace_back(
            se.time + synaptic_delay, se.id);
      }
    }
    // Sort events. Not necessary in this constant delay case.
    /*
    for (int i = 0; i < p_neu_pop->n_neurons(); i++) {
      std::sort(se_que_vec[i].begin(), se_que_vec[i].end());
    }
    */
    t += dt;
  }
};

class NeuronSimulatorBigNetDelay: public NeuronSimulatorBigDelay
{
public:
  NeuronSimulatorBigNetDelay(const TyNeuronalParams &_pm, double _dt, double t0)
    : NeuronSimulatorBigDelay(_pm, _dt, t0)
  {}

  void NextDt(NeuronPopulationBase * p_neu_pop,
      TySpikeEventVec &ras, std::vector< size_t > &vec_n_spike) override
  {
    const auto & synaptic_delay_net = *(p_neu_pop->SynapticDelayNet());
    TySpikeEventVec spike_events;
    poisson_time_vec.SaveIdxAndClean();

    // Go one step.
    NextStepNoInteract(p_neu_pop, spike_events, dt);

    for (auto const &se : spike_events) {
      vec_n_spike[se.id]++;  // Count spikes.
    }

    std::sort(spike_events.begin(), spike_events.end());
    ras.insert(ras.end(), spike_events.begin(), spike_events.end());

    // Insert new synaptic events.
    const auto &net = p_neu_pop->GetNeuronalParamsPtr()->net;
    for (auto const &se : spike_events) {
      // Loop over affected neurons.
      for (SparseMat::InnerIterator it(net, se.id),
                                    delay_it(synaptic_delay_net, se.id);
           it; ++it, ++delay_it) {
        se_que_vec[it.row()].emplace_back(
            se.time + delay_it.value(), se.id);
      }
    }
    // Sort events.
    for (int i = 0; i < p_neu_pop->n_neurons(); i++) {
      std::sort(se_que_vec[i].begin(), se_que_vec[i].end());
    }
    t += dt;
  }
};

#endif
