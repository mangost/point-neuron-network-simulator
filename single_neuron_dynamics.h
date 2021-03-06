#ifndef HEADER_SINGLE_NEURON_DYNAMICS
#define HEADER_SINGLE_NEURON_DYNAMICS

#include "common_header.h"
#include "math_helper.h"

// Fast code for exp(x)
#include "fmath.hpp"
//#define exp(x) fmath::expd(x)

template<typename Ty>
inline Ty my_expd(const Ty &x)
{ return exp(x); }

template<>
inline double my_expd(const double &x)
{ return fmath::expd(x); }

#define exp(x) my_expd(x)

struct Ty_Neuron_Dym_Base
{
  virtual double Get_V_threshold() const = 0;
  virtual int Get_id_V() const = 0;
  virtual int Get_id_gE() const = 0;
  virtual int Get_id_gI() const = 0;
  virtual int Get_id_gEInject() const = 0;  // index of gE injection variable
  virtual int Get_id_gIInject() const = 0;  // index of gI injection variable
  virtual int Get_n_dym_vars() const = 0;   // number of state variables
  virtual void Set_Time_Refractory(double t_ref) = 0;
  virtual void NextStepSingleNeuronQuiet(
    double *dym_val,
    double &t_in_refractory,
    double &spike_time_local,
    double dt_local) const = 0;
  // TODO: Need a better way to pass external current to the neuron.
  //       If only pass a constant current, the simulation accuracy will be a bit low.
  //       If pass a callable object, then we either need to
  //         a) Use template for neuron model in NeuronPopulation (like now).
  //         b) Use virtual class to hold the external data, but that will hurt speed.
  //            Probably use (void*) ?
  virtual void VoltHandReset(double *dym_val) const = 0;
  virtual const double * Get_dym_default_val() const = 0;
};

struct Ty_LIF_G_core
{
  // The neuron model named LIF-G in this code.
  // This is the Leaky Integrate-and-Fire model with jump conductance.
  double V_threshold  = 1.0;  // voltages are in dimensionless unit
  double V_reset      = 0.0;
  double V_leakage    = 0.0;
  double V_excitatory = 14.0/3.0;
  double V_inhibitory = -2.0/3.0;
  double G_leak       = 0.05;  // ms^-1
  double tau_gE       = 2.0;   // ms
  double tau_gI       = 5.0;   // ms
  double Time_Refractory = 2.0;   // ms
  static const int n_var = 3;  // number of dynamical variables
  static const int id_V  = 0;  // index of V variable
  static const int id_gE = 1;  // index of gE variable
  static const int id_gI = 2;  // index of gI variable
  static const int id_gEInject = id_gE;  // index of gE injection variable
  static const int id_gIInject = id_gI;  // index of gI injection variable

  // Evolve conductance only
  void NextDtConductance(double *dym_val, double dt) const
  {
    dym_val[id_gE] *= exp(-dt / tau_gE);
    dym_val[id_gI] *= exp(-dt / tau_gI);
  }

  // Get instantaneous dv/dt for current dynamical state
  double GetDv(const double *dym_val) const
  {
    return - G_leak * (dym_val[id_V] - V_leakage)
           - dym_val[id_gE] * (dym_val[id_V] - V_excitatory)
           - dym_val[id_gI] * (dym_val[id_V] - V_inhibitory);
  }

  // Evolve the state `dym_val' a `dt' forward,
  // using classical Runge–Kutta 4-th order scheme for voltage.
  // Conductance will evolve using the exact formula.
  // Return derivative k1 at t_n, for later interpolation.
  MACRO_NO_INLINE double DymInplaceRK4(double *dym_val, double dt) const
  {
    double v_n = dym_val[id_V];
    double k1, k2, k3, k4;
    double exp_E = exp(-0.5 * dt / tau_gE);
    double exp_I = exp(-0.5 * dt / tau_gI);

    // k1 = f(t_n, y_n)
    k1 = GetDv(dym_val);

    // y_n + 0.5*k1*dt
    dym_val[id_V ] = v_n + 0.5 * dt * k1;
    dym_val[id_gE] *= exp_E;
    dym_val[id_gI] *= exp_I;
    // k2 = f(t+dt/2, y_n + 0.5*k1*dt)
    k2 = GetDv(dym_val);

    // y_n + 0.5*k2*dt
    dym_val[id_V ] = v_n + 0.5 * dt * k2;
    // k3 = f(t+dt/2, y_n + 0.5*k2*dt)
    k3 = GetDv(dym_val);

    // y_n + k3*dt
    dym_val[id_V ] = v_n + dt * k3;
    dym_val[id_gE] *= exp_E;
    dym_val[id_gI] *= exp_I;
    // k4 = f(t+dt, y_n + k3*dt)
    k4 = GetDv(dym_val);

    dym_val[id_V ] = v_n + dt/6.0 * (k1 + 2*k2 + 2*k3 + k4);

    return k1;
  }
};

struct Ty_LIF_GH_core
{
  // The neuron model named LIF-GH in this code.
  // This is the Leaky Integrate-and-Fire model with order 1 smoothed conductance.
  double V_threshold  = 1.0;  // voltages are in dimensionless unit
  double V_reset      = 0.0;
  double V_leakage    = 0.0;
  double V_excitatory = 14.0/3.0;
  double V_inhibitory = -2.0/3.0;
  double G_leak       = 0.05;  // ms^-1
  double tau_gE       = 2.0;   // ms
  double tau_gE_s1    = 0.5;   // ms
  double tau_gI       = 5.0;   // ms
  double tau_gI_s1    = 0.8;   // ms
  double Time_Refractory = 2.0;   // ms   
  static const int n_var    = 5;  // number of dynamical variables
  static const int id_V     = 0;  // index of V variable
  static const int id_gE    = 1;  // index of gE variable
  static const int id_gI    = 2;  // index of gI variable
  static const int id_gE_s1 = 3;  // index of derivative of gE
  static const int id_gI_s1 = 4;  // index of derivative of gI
  static const int id_gEInject = id_gE_s1;  // index of gE injection variable
  static const int id_gIInject = id_gI_s1;  // index of gI injection variable

  // Evolve conductance only
  void NextDtConductance(double *dym_val, double dt) const
  {
    /**
      ODE:
        g '[t] == -1/tC  * g [t] + gR[t]
        gR'[t] == -1/tCR * gR[t]
      Solution:
        g [t] = exp(-t/tC) * g[0] + (exp(-t/tC) - exp(-t/tCR)) * gR[0] * tC * tCR / (tC - tCR)
        gR[t] = exp(-t/tCR) * gR[0]
      Another form (hopefully more accurate, note exp(x)-1 is expm1(x) ):
        g [t] = exp(-t/tC) * (g[0] + gR[0] * (exp((1/tC-1/tCR)*t) - 1) / (1/tC - 1/tCR))
      Or
        g [t] = exp(-t/tC) * g[0] + exp(-t/tCR) * gR[0] * (exp((1/tCR-1/tC)*t) - 1) / (1/tCR-1/tC)
    */
    // Excitatory
    double expC  = exp(-dt / tau_gE);
    double expCR = exp(-dt / tau_gE_s1);
    dym_val[id_gE] = expC*dym_val[id_gE] + (expC - expCR) * dym_val[id_gE_s1] * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
    dym_val[id_gE_s1] *= expCR;
    // Inhibitory
    expC  = exp(-dt / tau_gI);
    expCR = exp(-dt / tau_gI_s1);
    dym_val[id_gI] = expC*dym_val[id_gI] + (expC - expCR) * dym_val[id_gI_s1] * tau_gI * tau_gI_s1 / (tau_gI - tau_gI_s1);
    dym_val[id_gI_s1] *= expCR;
  }

  // Get instantaneous dv/dt for current dynamical state
  inline double GetDv(const double *dym_val) const
  {
    return - G_leak * (dym_val[id_V] - V_leakage)
           - dym_val[id_gE] * (dym_val[id_V] - V_excitatory)
           - dym_val[id_gI] * (dym_val[id_V] - V_inhibitory);
  }

  // Evolve the state `dym_val' a `dt' forward,
  // using classical Runge–Kutta 4-th order scheme for voltage.
  // Conductance will evolve using the exact formula.
  // Return derivative k1 at t_n, for later interpolation.
  MACRO_NO_INLINE double DymInplaceRK4(double *dym_val, double dt) const
  {
    double v_n = dym_val[id_V];
    double k1, k2, k3, k4;
    double expEC  = exp(-0.5 * dt / tau_gE);  // TODO: maybe cache this value?
    double expECR = exp(-0.5 * dt / tau_gE_s1);
    double expIC  = exp(-0.5 * dt / tau_gI);
    double expICR = exp(-0.5 * dt / tau_gI_s1);
    double gE_s_coef = (expEC - expECR) * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
    double gI_s_coef = (expIC - expICR) * tau_gI * tau_gI_s1 / (tau_gI - tau_gI_s1);

    // k1 = f(t_n, y_n)
    k1 = GetDv(dym_val);

    // y_n + 0.5*k1*dt
    dym_val[id_V ] = v_n + 0.5 * dt * k1;
    dym_val[id_gE] = expEC * dym_val[id_gE] + gE_s_coef * dym_val[id_gE_s1];
    dym_val[id_gI] = expIC * dym_val[id_gI] + gI_s_coef * dym_val[id_gI_s1];
    dym_val[id_gE_s1] *= expECR;
    dym_val[id_gI_s1] *= expICR;
    // k2 = f(t+dt/2, y_n + 0.5*k1*dt)
    k2 = GetDv(dym_val);

    // y_n + 0.5*k2*dt
    dym_val[id_V ] = v_n + 0.5 * dt * k2;
    // k3 = f(t+dt/2, y_n + 0.5*k2*dt)
    k3 = GetDv(dym_val);

    // y_n + k3*dt
    dym_val[id_V ] = v_n + dt * k3;
    dym_val[id_gE] = expEC * dym_val[id_gE] + gE_s_coef * dym_val[id_gE_s1];
    dym_val[id_gI] = expIC * dym_val[id_gI] + gI_s_coef * dym_val[id_gI_s1];
    dym_val[id_gE_s1] *= expECR;
    dym_val[id_gI_s1] *= expICR;
    // k4 = f(t+dt, y_n + k3*dt)
    k4 = GetDv(dym_val);

    dym_val[id_V ] = v_n + dt/6.0 * (k1 + 2*k2 + 2*k3 + k4);

    return k1;
  }
};

// Adapter for IF type models (only sub-threshold dynamics and need hand reset)
template<typename TyNeuronModel>
struct Ty_LIF_stepper: public TyNeuronModel, public Ty_Neuron_Dym_Base
{
  // for template class we need these "using"s. It's a requirement for TyNeuronModel.
  using TyNeuronModel::id_V;
  using TyNeuronModel::id_gE;
  using TyNeuronModel::V_threshold;
  using TyNeuronModel::V_reset;
  using TyNeuronModel::Time_Refractory;
  using TyNeuronModel::GetDv;
  using TyNeuronModel::NextDtConductance;
  using TyNeuronModel::DymInplaceRK4;

  using TyNeuronModel::id_gEInject;
  using TyNeuronModel::id_gIInject;
  using TyNeuronModel::n_var;
  using TyNeuronModel::id_gI;
  double Get_V_threshold() const override {return V_threshold;};
  int Get_id_gEInject() const override {return id_gEInject;}
  int Get_id_gIInject() const override {return id_gIInject;}
  int Get_n_dym_vars() const override {return n_var;}
  int Get_id_V() const override {return id_V;}
  int Get_id_gE() const override {return id_gE;}
  int Get_id_gI() const override {return id_gI;}

  void Set_Time_Refractory(double t_ref) override
  {
    if (t_ref>=0) {
      Time_Refractory = t_ref;  // No error checking
    } else {
      cerr << "Setting Time_Refractory < 0 : t_ref = " << t_ref << "\n";
    }
  }

  // Used when reset the voltage by hand. (e.g. outside this class)
  inline void VoltHandReset(double *dym_val) const override
  {
    dym_val[id_V] = V_reset;
  }

  void SpikeTimeRefine(double *dym_t, double &t_spike, int n_it) const
  {
    if (n_it == 1) {
      DymInplaceRK4(dym_t, t_spike);
      t_spike -= (dym_t[id_V]-V_threshold) / GetDv(dym_t);
    } else {  // multiple iterations
      double dym_t0[n_var];
      std::copy(dym_t, dym_t+n_var, dym_t0);  // copy of init state
      for (int i=n_it-1; i>=0; i--) {
        DymInplaceRK4(dym_t, t_spike);
        /*t_spike -= (dym_t[id_V]-V_threshold) / GetDv(dym_t);*/
        double dt_spike = (dym_t[id_V]-V_threshold) / GetDv(dym_t);
        t_spike -= dt_spike;
        dbg_printf("SpikeTimeRefine(): dt = %.17g, t_spike = %.17g, V = %.17g, Dv = %g\n",
            dt_spike, t_spike, dym_t[id_V], GetDv(dym_t));
        if (i) std::copy(dym_t0, dym_t0+n_var, dym_t);
      }
    }
  }

  // Evolve the ODE and note down the spike time, assuming no reset and no external input.
  // `spike_time_local' should be guaranteed to be within [0, dt] or NAN.
  MACRO_NO_INLINE void NextStepSingleNeuronContinuous(double *dym_val, double &spike_time_local, double dt) const
  {
    double dym_t[n_var];
    std::copy(dym_val, dym_val+n_var, dym_t);
    double v_n = dym_val[id_V];
    double k1  = DymInplaceRK4(dym_val, dt);

    if (v_n <= V_threshold
        && dym_val[id_V ] > V_threshold) {
      spike_time_local = cubic_hermit_real_root(dt,
        v_n, dym_val[id_V ],
        k1, GetDv(dym_val), V_threshold);
      // refine spike time
      /*SpikeTimeRefine(dym_t, spike_time_local, 2);*/
    } else {
      if (v_n > 0.996 && k1>0) { // the v_n > 0.996 is for dt=0.5 ms, LIF,G model
        // Try capture some missing spikes that the intermediate value passes
        // threshold, but both ends are lower than threshold.
        // Get quadratic curve from value of both ends and derivative from left end
        // Return the maximum point as `t_max_guess'
        double c = v_n;
        double b = k1;
        double a = (dym_val[id_V ] - c - b*dt)/(dt*dt);
        double t_max_guess = -b/(2*a);
        // In LIF-G, it can guarantee that a<0 (concave),
        // hence t_max_guess > 0. But in LIF-G model, we still need to
        // check 0 < t_max_guess
        if (0 < t_max_guess && t_max_guess < dt
            && (b*b)/(-4*a)+c >= V_threshold) {
          //dbg_printf("Rare event: mid-dt spike captured, guess time: %f\n", t_max_guess);
          dbg_printf("NextStepSingleNeuronContinuous(): possible mid-dt spike detected:\n");
          dbg_printf("  Guessed max time: %f, dt = %f\n", t_max_guess, dt);
          // root should in [0, t_max_guess]
          spike_time_local = cubic_hermit_real_root(dt,
            v_n, dym_val[id_V ],
            k1, GetDv(dym_val), V_threshold);
          /*SpikeTimeRefine(dym_t, spike_time_local, 2);*/
        } else {
          spike_time_local = std::numeric_limits<double>::quiet_NaN();
        }
      } else {
        spike_time_local = std::numeric_limits<double>::quiet_NaN();
      }
    }
  }

  // Evolve single neuron as if no external input.
  // Return first spike time in `spike_time_local', if any.
  void NextStepSingleNeuronQuiet(double *dym_val, double &t_in_refractory,
                           double &spike_time_local, double dt_local) const override
  {
    //! at most one spike allowed during this dt_local
    if (t_in_refractory == 0) {
      dbg_printf("NextStepSingleNeuronQuiet(): dt_local = %.17g\n", dt_local);
      dbg_printf("NextStepSingleNeuronQuiet(): begin state=%.16e,%.16e,%.16e\n",
                 dym_val[0], dym_val[1], dym_val[2]);
      NextStepSingleNeuronContinuous(dym_val, spike_time_local, dt_local);
      dbg_printf("NextStepSingleNeuronQuiet(): end   state=%.16e,%.16e,%.16e\n",
                 dym_val[0], dym_val[1], dym_val[2]);
      if (!std::isnan(spike_time_local)) {
        // Add `numeric_limits<double>::min()' to make sure t_in_refractory > 0.
        t_in_refractory = dt_local - spike_time_local
                          + std::numeric_limits<double>::min();
        dym_val[id_V] = V_reset;
        dbg_printf("NextStepSingleNeuronQuiet(): neuron fired, spike_time_local = %.17g\n", spike_time_local);
        if (t_in_refractory >= Time_Refractory) {
          // Short refractory period (< dt_local), neuron will be actived again.
          dt_local = t_in_refractory - Time_Refractory;
          t_in_refractory = 0;
          // Back to the activation time.
          NextDtConductance(dym_val, -dt_local);
          double spike_time_local_tmp;
          NextStepSingleNeuronContinuous(dym_val, spike_time_local_tmp, dt_local);
          if (!std::isnan(spike_time_local_tmp)) {
            cerr << "NextStepSingleNeuronQuiet(): Multiple spikes in one dt. Interaction dropped." << endl;
            cerr << "  dt_local = " << dt_local << '\n';
            cerr << "  spike_time_local = " << spike_time_local << '\n';
            cerr << "  t_in_refractory = " << t_in_refractory << '\n';
            cerr << "  dym_val[id_V] = " << dym_val[id_V] << '\n';
            cerr << "  dym_val[id_gE] = " << dym_val[id_gE] << '\n';
            throw "Multiple spikes in one dt.";
          }
        }
      }
    } else {
      // Neuron in refractory period.
      double dt_refractory_remain = Time_Refractory
                                 - t_in_refractory;
      if (dt_refractory_remain < dt_local) {
        // neuron will awake after dt_refractory_remain which is in this dt_local
        NextDtConductance(dym_val, dt_refractory_remain);
        assert( dym_val[id_V] == V_reset );
        t_in_refractory = 0;
        NextStepSingleNeuronQuiet(dym_val, t_in_refractory,
                            spike_time_local, dt_local - dt_refractory_remain);
      } else {
        spike_time_local = std::numeric_limits<double>::quiet_NaN();
        NextDtConductance(dym_val, dt_local);
        t_in_refractory += dt_local;
      }
    }
  }

  const double * Get_dym_default_val() const override
  {
     static const double dym[n_var] = {0};
     return dym;
  }
};

typedef Ty_LIF_stepper<Ty_LIF_G_core>  Ty_LIF_G;
typedef Ty_LIF_stepper<Ty_LIF_GH_core> Ty_LIF_GH;

/////////////////////////////////////////////////////////////////////////////
// Model of classical Hodgkin-Huxley (HH) neuron,
// with two ODE for G (See Ty_LIF_GH_core::NextDtConductance() for details).
// The parameters for G comes from unknown source.
template<typename ExtraCurrent>
struct Ty_HH_GH_CUR_core
  :public Ty_Neuron_Dym_Base
{
  using Ty_Neuron_Dym_Base::NextStepSingleNeuronQuiet;
  typedef typename ExtraCurrent::TyData TyCurrentData;

  double V_Na = 115;             // mV
  double V_K  = -12;
  double V_L  =  10.6;
  double G_Na = 120;             // mS cm^-2
  double G_K  =  36;
  double G_L  =   0.3;
  double V_gE =  65;             // mV, for synaptic
  double V_gI = -15;             // mV
  double tau_gE    = 0.5;        // ms, decay time for gE equation
  double tau_gE_s1 = 3.0;        // ms, decay time for gE_H equation.
  double tau_gI    = 0.5;        // ms
  double tau_gI_s1 = 7.0;        // ms
  double V_threshold = 65;       // mV, determine the spike time for synaptic interaction.
  //double Time_Refractory = 1.0;  // ms, hard refractory period. Used for correctly locate the firing when change time step
  static const int n_var = 8;
  static const int n_var_soma = 4;  // number of variables for non- G part
  static const int id_V     = 0;    // id_V should just before gating variables(see main.cpp)
  static const int id_h     = 1;
  static const int id_m     = 2;
  static const int id_n     = 3;
  static const int id_gE    = 4;
  static const int id_gI    = 5;
  static const int id_gE_s1 = 6;
  static const int id_gI_s1 = 7;
  static const int id_gEInject = id_gE_s1;
  static const int id_gIInject = id_gI_s1;

  // dV/dt term
  inline double GetDv(const double *dym_val, double t,
               const TyCurrentData &extra_data) const
  {
    const double &V = dym_val[id_V];
    const double &h = dym_val[id_h];
    const double &m = dym_val[id_m];
    const double &n = dym_val[id_n];
    /*printf("new. extra_data: %e\n", ExtraCurrent()(t, extra_data));*/
    return
      -(V-V_Na) * G_Na * h * m * m * m
      -(V-V_K ) * G_K * n * n * n * n
      -(V-V_L ) * G_L
      -(V-V_gE) * dym_val[id_gE]
      -(V-V_gI) * dym_val[id_gI]
      + ExtraCurrent()(t, extra_data);
  }

//  // Classical HH neuron equations go here
//  void ODE_RHS(const double *dym_val, double *dym_d_val) const
//  {
//    const double &V = dym_val[id_V];
//    const double &h = dym_val[id_h];
//    const double &m = dym_val[id_m];
//    const double &n = dym_val[id_n];
//    dym_d_val[id_V] = GetDv(dym_val);
//    dym_d_val[id_h] = (1-h) * (0.07*exp(-V/20))
//                        - h / (exp(3-0.1*V)+1);
//    dym_d_val[id_m] = (1-m) * ((2.5-0.1*V)/(exp(2.5-0.1*V)-1))
//                        - m * (4*exp(-V/18));
//    dym_d_val[id_n] = (1-n) * ((0.1-0.01*V)/(exp(1-0.1*V)-1))
//                        - n * (0.125*exp(-V/80));
//    // No RHS for G here, that is solved explicitly.
//  }

  // Classical HH neuron equations go here. Faster version.
  void ODE_RHS(const double *dym_val, double *dym_d_val, double t,
               const TyCurrentData &extra_data) const
  {
    const double &V = dym_val[id_V];
    const double &h = dym_val[id_h];
    const double &m = dym_val[id_m];
    const double &n = dym_val[id_n];
    dym_d_val[id_V] = GetDv(dym_val, t, extra_data);
    double e1 = exp(-0.1*dym_val[id_V]);
    double e2 = sqrt(e1);
    dym_d_val[id_h] = (1-h) * (0.07*e2)
                        - h / (e1*exp(3.0)+1);
    dym_d_val[id_m] = (1-m) * ((2.5-0.1*V)/(e1*exp(2.5)-1))
                        - m * (4*exp(-V/18));
    dym_d_val[id_n] = (1-n) * ((0.1-0.01*V)/(e1*exp(1.0)-1))
                        - n * (0.125*sqrt(sqrt(e2)));
    // No RHS for G here, that is solved explicitly.
  }

  double DymInplaceRK4(double *dym_val, double dt, double t,
                       const TyCurrentData &extra_data) const
  {
    // for G. See Ty_LIF_GH_core::NextDtConductance().
    double expEC  = exp(-0.5 * dt / tau_gE);
    double expECR = exp(-0.5 * dt / tau_gE_s1);
    double expIC  = exp(-0.5 * dt / tau_gI);
    double expICR = exp(-0.5 * dt / tau_gI_s1);
    double gE_s_coef = (expEC - expECR) * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
    double gI_s_coef = (expIC - expICR) * tau_gI * tau_gI_s1 / (tau_gI - tau_gE_s1);

    double k1[n_var_soma], k2[n_var_soma], k3[n_var_soma], k4[n_var_soma];
    double dym_val0[n_var_soma];
    // Use template BLAS lib, so some expressions can be written in vector form.
    typedef Eigen::Map< Eigen::RowVectorXd > TyMapVec;
    TyMapVec dym_val0_v(dym_val0, n_var_soma);
    TyMapVec dym_val_v (dym_val , n_var_soma);
    TyMapVec k1_v(k1, n_var_soma);
    TyMapVec k2_v(k2, n_var_soma);
    TyMapVec k3_v(k3, n_var_soma);
    TyMapVec k4_v(k4, n_var_soma);

    dym_val0_v = dym_val_v;
    ODE_RHS(dym_val, k1, t, extra_data);

    dym_val_v = dym_val0_v + 0.5*dt*k1_v;
    dym_val[id_gE] = expEC*dym_val[id_gE] + gE_s_coef*dym_val[id_gE_s1];
    dym_val[id_gI] = expIC*dym_val[id_gI] + gI_s_coef*dym_val[id_gI_s1];
    dym_val[id_gE_s1] *= expECR;
    dym_val[id_gI_s1] *= expICR;
    ODE_RHS(dym_val, k2, t + 0.5*dt, extra_data);

    dym_val_v = dym_val0_v + 0.5*dt*k2_v;
    ODE_RHS(dym_val, k3, t + 0.5*dt, extra_data);

    dym_val_v = dym_val0_v + dt*k3_v;
    dym_val[id_gE] = expEC*dym_val[id_gE] + gE_s_coef*dym_val[id_gE_s1];
    dym_val[id_gI] = expIC*dym_val[id_gI] + gI_s_coef*dym_val[id_gI_s1];
    dym_val[id_gE_s1] *= expECR;
    dym_val[id_gI_s1] *= expICR;
    ODE_RHS(dym_val, k4, t + dt, extra_data);

    dym_val_v = dym_val0_v + dt/6.0 * (k1_v + 2*k2_v + 2*k3_v + k4_v);

    return k1[id_V];
  };

  void NextStepSingleNeuronQuiet(
    double *dym_val,
    double &t_in_refractory,
    double &spike_time_local,
    double dt_local,
    double t,
    const TyCurrentData &extra_data) const
  {
    /*printf("new. extra_data: %e\n", ExtraCurrent()(t, extra_data));*/
    spike_time_local = std::numeric_limits<double>::quiet_NaN();
    double v0 = dym_val[id_V];
    double k1 = DymInplaceRK4(dym_val, dt_local, t, extra_data);
    double &v1 = dym_val[id_V];
    // See if neuron is firing. t_in_refractory == 0 means the neuron
    // is not in hand set refractory period, avoids kind of infinite loop.
    if (v0 < V_threshold && v1 >= V_threshold && t_in_refractory == 0) {
      // In a rare case which v0, v1 <= V_threshold, it is still possible 
      // that the 'in-dt' volt exceeds V_threshold. I ignored that case here.
      // Some redundant calculation of GetDv(). Let's leave it.
      spike_time_local = cubic_hermit_real_root(dt_local,
        v0, v1, k1, GetDv(dym_val, t, extra_data), V_threshold);
    }
    if (dt_local>0) {
      // not 100% mathematically safe. Use the hard refractory for that.
      t_in_refractory = 0;
    }
    //t_in_refractory += dt_local;
    //if (t_in_refractory > Time_Refractory) {
    //t_in_refractory = 0;
    //}
  }

  const double * Get_dym_default_val() const override
  {
    static const double dym_default[n_var] = {
      2.7756626542950876e-04, 5.9611104634682788e-01,
      5.2934217620863984e-02, 3.1768116757978115e-01, 0, 0, 0, 0 };
    return dym_default;
  }
};

// Model that note down spike event when the spike is falling.
template<typename ExtraCurrent>
struct Ty_HH_FT_GH_CUR_core  // Falling threshold
  :public Ty_HH_GH_CUR_core<ExtraCurrent>
{
  typedef Ty_HH_GH_CUR_core<ExtraCurrent> T0;
  typedef typename T0::TyCurrentData TyCurrentData;
  using T0::id_V;
  using T0::V_threshold;
  using T0::DymInplaceRK4;
  using T0::GetDv;

  void NextStepSingleNeuronQuiet(
    double *dym_val,
    double &t_in_refractory,
    double &spike_time_local,
    double dt_local,
    double t,
    const TyCurrentData &extra_data) const
  {
    spike_time_local = std::numeric_limits<double>::quiet_NaN();
    double v0 = dym_val[id_V];
    double k1 = DymInplaceRK4(dym_val, dt_local, t, extra_data);
    double &v1 = dym_val[id_V];
    if (v0 > V_threshold && v1 <= V_threshold && t_in_refractory == 0) {
      spike_time_local = cubic_hermit_real_root(dt_local,
        v0, v1, k1, GetDv(dym_val, t, extra_data), V_threshold);
    }
    if (dt_local>0) {
      t_in_refractory = 0;
    }
  }
};

// Model that note down spike event when the spike is at the peak.
template<typename ExtraCurrent>
struct Ty_HH_PT_GH_CUR_core  // Peak threshold
  :public Ty_HH_GH_CUR_core<ExtraCurrent>
{
  typedef Ty_HH_GH_CUR_core<ExtraCurrent> T0;
  typedef typename T0::TyCurrentData TyCurrentData;
  using T0::id_V;
  using T0::V_threshold;
  using T0::DymInplaceRK4;
  using T0::GetDv;

  void NextStepSingleNeuronQuiet(
    double *dym_val,
    double &t_in_refractory,
    double &spike_time_local,
    double dt_local,
    double t,
    const TyCurrentData &extra_data) const
  {
    spike_time_local = std::numeric_limits<double>::quiet_NaN();
    double v0 = dym_val[id_V];
    double k0 = DymInplaceRK4(dym_val, dt_local, t, extra_data);
    if (v0 >= V_threshold) {
      double k1 = GetDv(dym_val, t, extra_data);
      if (k0>=0 && k1<0 && t_in_refractory == 0) {
        double v1 = dym_val[id_V];
        spike_time_local = cubic_hermit_real_peak(dt_local,
            v0, v1, k0, k1);
        /*spike_time_local = k0/(k0-k1) * dt_local;*/
      }
    }
    if (dt_local>0) {
      t_in_refractory = 0;
    }
  }
};

template<typename ExtraCurrent>
struct Ty_HH_dPT_GH_CUR_core  // Peak threshold
  :public Ty_HH_GH_CUR_core<ExtraCurrent>
{
  typedef Ty_HH_GH_CUR_core<ExtraCurrent> T0;
  typedef typename T0::TyCurrentData TyCurrentData;
  using T0::id_V;
  using T0::id_h;
  using T0::id_m;
  using T0::id_n;
  using T0::id_gE;
  using T0::id_gI;
  using T0::id_gE_s1;
  using T0::id_gI_s1;
  using T0::n_var;
  using T0::n_var_soma;
  using T0::V_Na;
  using T0::V_K;
  using T0::V_L;
  using T0::V_gE;
  using T0::V_gI;
  using T0::G_Na;
  using T0::G_K;
  using T0::G_L;
  using T0::tau_gE;
  using T0::tau_gI;
  using T0::V_threshold;
  using T0::DymInplaceRK4;
  using T0::GetDv;
  using T0::ODE_RHS;

  // V''(t)
  inline double GetDDv(const double *dym_val,
      const double *dym_d_val,
      double t,
      const TyCurrentData &extra_data) const
  {
    const double &V = dym_val[id_V];
    const double &h = dym_val[id_h];
    const double &m = dym_val[id_m];
    const double &n = dym_val[id_n];
    const double &dV = dym_d_val[id_V];
    const double &dh = dym_d_val[id_h];
    const double &dm = dym_d_val[id_m];
    const double &dn = dym_d_val[id_n];
    double dgE = -1.0 / tau_gE * dym_val[id_gE] + dym_val[id_gE_s1];
    double dgI = -1.0 / tau_gI * dym_val[id_gI] + dym_val[id_gI_s1];
    /*printf("new. extra_data: %e\n", ExtraCurrent()(t, extra_data));*/
    // FIXME: Better way to do this?
    double dext = (ExtraCurrent()(t+1e-7, extra_data)
                 - ExtraCurrent()(t-1e-7, extra_data)) / 2e-7;
    return
      -dV       * G_Na * h  * m * m * m
      -(V-V_Na) * G_Na * dh * m * m * m
      -(V-V_Na) * G_Na * h  * m * m * 3.0 * dm
      -dV       * G_K * n * n * n * n
      -(V-V_K ) * G_K * n * n * n * dn * 4.0
      -dV       * G_L
      -dV       * dym_val[id_gE]
      -(V-V_gE) * dgE
      -dV       * dym_val[id_gI]
      -(V-V_gI) * dgI
      + dext;
  }

  void NextStepSingleNeuronQuiet(
    double *dym_val,
    double &t_in_refractory,
    double &spike_time_local,
    double dt_local,
    double t,
    const TyCurrentData &extra_data) const
  {
    spike_time_local = std::numeric_limits<double>::quiet_NaN();
    double v0 = dym_val[id_V];
    double dym_val0[n_var];
    memcpy(dym_val0, dym_val, sizeof(dym_val0));
    double k0 = DymInplaceRK4(dym_val, dt_local, t, extra_data);
    if (v0 >= V_threshold) {
      double k1 = GetDv(dym_val, t, extra_data);
      if (k0>=0 && k1<0 && t_in_refractory == 0) {
        double dym_d_val0[n_var_soma];
        double dym_d_val[n_var_soma];
        ODE_RHS(dym_val0, dym_d_val0, t, extra_data);
        ODE_RHS(dym_val , dym_d_val , t+dt_local, extra_data);
        double dd0 = GetDDv(dym_val0, dym_d_val0, t, extra_data);
        double dd1 = GetDDv(dym_val,  dym_d_val,  t+dt_local, extra_data);
        spike_time_local = cubic_hermit_real_root(dt_local,
            k0, k1, dd0, dd1, 0);
      }
    }
    if (dt_local>0) {
      t_in_refractory = 0;
    }
  }
};

// Model of classical Hodgkin-Huxley (HH) neuron,
// with one ODE for G (See Ty_LIF_G_core::NextDtConductance() for details).
template<typename ExtraCurrent>
struct Ty_HH_G_CUR_core
  :public Ty_HH_GH_CUR_core<ExtraCurrent>
{
  typedef typename Ty_HH_GH_CUR_core<ExtraCurrent>::TyCurrentData TyCurrentData;
  typedef Ty_HH_GH_CUR_core<ExtraCurrent> T0;
  using T0::id_V;
  using T0::id_gE;
  using T0::id_gI;
  using T0::n_var_soma;
  using T0::V_threshold;
  using T0::GetDv;
  using T0::ODE_RHS;

  double tau_gE = 3.0;
  double tau_gI = 7.0;
  static const int n_var = 6;
  static const int id_gEInject = id_gE;
  static const int id_gIInject = id_gI;

  double DymInplaceRK4(double *dym_val, double dt, double t,
                       const TyCurrentData &extra_data) const
  {
    double exp_E = exp(-0.5 * dt / tau_gE);
    double exp_I = exp(-0.5 * dt / tau_gI);

    double k1[n_var_soma], k2[n_var_soma], k3[n_var_soma], k4[n_var_soma];
    double dym_val0[n_var_soma];
    // Use template BLAS lib
    // so some expressions can be written in vector form.
    typedef Eigen::Map< Eigen::RowVectorXd > TyMapVec;
    TyMapVec dym_val0_v(dym_val0, n_var_soma);
    TyMapVec dym_val_v (dym_val , n_var_soma);
    TyMapVec k1_v(k1, n_var_soma);
    TyMapVec k2_v(k2, n_var_soma);
    TyMapVec k3_v(k3, n_var_soma);
    TyMapVec k4_v(k4, n_var_soma);

    dym_val0_v = dym_val_v;
    ODE_RHS(dym_val, k1, t, extra_data);

    dym_val_v = dym_val0_v + 0.5*dt*k1_v;
    dym_val[id_gE] *= exp_E;
    dym_val[id_gI] *= exp_I;
    ODE_RHS(dym_val, k2, t + 0.5*dt, extra_data);

    dym_val_v = dym_val0_v + 0.5*dt*k2_v;
    ODE_RHS(dym_val, k3, t + 0.5*dt, extra_data);

    dym_val_v = dym_val0_v + dt*k3_v;
    dym_val[id_gE] *= exp_E;
    dym_val[id_gI] *= exp_I;
    ODE_RHS(dym_val, k4, t + dt, extra_data);

    dym_val_v = dym_val0_v + dt/6.0 * (k1_v + 2*k2_v + 2*k3_v + k4_v);

    return k1[id_V];
  };

  void NextStepSingleNeuronQuiet(
    double *dym_val,
    double &t_in_refractory,
    double &spike_time_local,
    double dt_local,
    double t,
    const TyCurrentData &extra_data) const
  {
    spike_time_local = std::numeric_limits<double>::quiet_NaN();
    double v0 = dym_val[id_V];
    double k1 = DymInplaceRK4(dym_val, dt_local, t, extra_data);
    double &v1 = dym_val[id_V];
    // See if neuron is firing. t_in_refractory == 0 means the neuron
    // is not in hand set refractory period, avoids kind of infinite loop.
    if (v0 < V_threshold && v1 >= V_threshold && t_in_refractory == 0) {
      spike_time_local = cubic_hermit_real_root(dt_local,
        v0, v1, k1, GetDv(dym_val, t, extra_data), V_threshold);
    }
    if (dt_local>0) {
      t_in_refractory = 0;
    }
  }
};

// Apply basic functions to the core HH model(adapter).
template<class TyNeuronModel>
struct Ty_HH_shell: public TyNeuronModel
{
  using TyNeuronModel::id_V;
  using TyNeuronModel::id_gE;
  using TyNeuronModel::id_gI;
  using TyNeuronModel::id_gEInject;
  using TyNeuronModel::id_gIInject;
  using TyNeuronModel::V_threshold;
  using TyNeuronModel::n_var;

  double Get_V_threshold() const override {return V_threshold;};
  int Get_id_gEInject() const override {return id_gEInject;}
  int Get_id_gIInject() const override {return id_gIInject;}
  int Get_n_dym_vars() const override {return n_var;}
  int Get_id_V() const override {return id_V;}
  int Get_id_gE() const override {return id_gE;}
  int Get_id_gI() const override {return id_gI;}
  void VoltHandReset(double *dym_val) const override
  {
    // no force reset required for HH
  }
  void Set_Time_Refractory(double t_ref) override
  {}
};

template<typename ExtraCurrent>
using Ty_HH_GH_CUR = Ty_HH_shell< Ty_HH_GH_CUR_core<ExtraCurrent> >;

template<typename ExtraCurrent>
using Ty_HH_FT_GH_CUR = Ty_HH_shell< Ty_HH_FT_GH_CUR_core<ExtraCurrent> >;

template<typename ExtraCurrent>
using Ty_HH_PT_GH_CUR = Ty_HH_shell< Ty_HH_PT_GH_CUR_core<ExtraCurrent> >;

template<typename ExtraCurrent>
using Ty_HH_G_CUR = Ty_HH_shell< Ty_HH_G_CUR_core<ExtraCurrent> >;

/////////////////////////////////////////////////////////////////////////////
// Current term

/// Template for zero current input
struct TyZeroCurrent
{
  typedef int TyData;
  double operator()(double t, const TyData &a) const
  { return 0.0; }
};

// Template that apply the current input
template<template<class> class Ty_Neuron_With_Current>
struct Neuron_Zero_Current_Adaper
    :public Ty_Neuron_With_Current<TyZeroCurrent>
{
  //using Ty_Neuron_With_Current<TyZeroCurrent>::NextStepSingleNeuronQuiet;

  void NextStepSingleNeuronQuiet(double *dym_val, double &t_in_refractory,
    double &spike_time_local, double dt_local) const override
  {
    Ty_Neuron_With_Current<TyZeroCurrent>::NextStepSingleNeuronQuiet(
        dym_val, t_in_refractory, spike_time_local, dt_local, 0, 0);
  }
};

/// Declare the model with zero current input
typedef Neuron_Zero_Current_Adaper< Ty_HH_GH_CUR >  Ty_HH_GH;
typedef Neuron_Zero_Current_Adaper< Ty_HH_FT_GH_CUR >  Ty_HH_FT_GH;
typedef Neuron_Zero_Current_Adaper< Ty_HH_PT_GH_CUR >  Ty_HH_PT_GH;
typedef Neuron_Zero_Current_Adaper< Ty_HH_G_CUR >  Ty_HH_G;

// Template for sine current input
struct TySineCurrent
{
  typedef double * TyData;  // Amplitude, angular frequency, phase
  double operator()(double t, const TyData &a) const
  { //printf("Current: %e, %e, %e\n", a[0], a[1], a[2]);  fflush(stdout);
    return a[0]*sin(a[1]*t + a[2]); }
};

// Template that apply the current input
template<template<class> class Ty_Neuron_With_Current>
struct Neuron_Sine_Current_Adaper
   :public Ty_Neuron_With_Current<TySineCurrent>
{
  using Ty_Neuron_With_Current<TySineCurrent>::NextStepSingleNeuronQuiet;

  void NextStepSingleNeuronQuiet(double *dym_val, double &t_in_refractory,
    double &spike_time_local, double dt_local) const override
  {
    fprintf(stderr, "Seems you are using a simulator that does not support extra current.\n");
    double d[3] = {0, 0, 0};
    Ty_Neuron_With_Current<TySineCurrent>::NextStepSingleNeuronQuiet(
        dym_val, t_in_refractory, spike_time_local, dt_local, 0, d);
  }
};

// Declare the models with sine current input
typedef Neuron_Sine_Current_Adaper< Ty_HH_GH_CUR > Ty_HH_GH_sine;
typedef Neuron_Sine_Current_Adaper< Ty_HH_FT_GH_CUR > Ty_HH_FT_GH_sine;
typedef Neuron_Sine_Current_Adaper< Ty_HH_PT_GH_CUR > Ty_HH_PT_GH_sine;
typedef Neuron_Sine_Current_Adaper< Ty_HH_G_CUR > Ty_HH_G_sine;

// Template for external current input
struct TyExtCurrent
{
  typedef double TyData;  // Amplitude, angular frequency, phase
  double operator()(double t, const TyData &a) const
  { //printf("Current: %e, %e, %e\n", a[0], a[1], a[2]);  fflush(stdout);
    return a;
  }
};

// Template that apply the current input
template<template<class> class Ty_Neuron_With_Current>
struct Neuron_Ext_Current_Adaper
   :public Ty_Neuron_With_Current<TyExtCurrent>
{
  using Ty_Neuron_With_Current<TyExtCurrent>::NextStepSingleNeuronQuiet;

  void NextStepSingleNeuronQuiet(double *dym_val, double &t_in_refractory,
    double &spike_time_local, double dt_local) const override
  {
    fprintf(stderr, "Seems you are using a simulator that does not support extra current.\n");
    double d = 0;
    Ty_Neuron_With_Current<TyExtCurrent>::NextStepSingleNeuronQuiet(
        dym_val, t_in_refractory, spike_time_local, dt_local, 0, d);
  }
};

// Declare the models with external current input
typedef Neuron_Ext_Current_Adaper< Ty_HH_GH_CUR > Ty_HH_GH_extI;
typedef Neuron_Ext_Current_Adaper< Ty_HH_FT_GH_CUR > Ty_HH_FT_GH_extI;
typedef Neuron_Ext_Current_Adaper< Ty_HH_PT_GH_CUR > Ty_HH_PT_GH_extI;
typedef Neuron_Ext_Current_Adaper< Ty_HH_G_CUR > Ty_HH_G_extI;

// HH model with continuous synaptic interaction
struct Ty_HH_GH_cont_syn
  :public Ty_Neuron_Dym_Base
{
  double V_Na = 11.5;
  double V_K  = -1.2;
  double V_L  =  1.06;
  double G_Na = 120;
  double G_K  =  36;
  double G_L  =   0.3;
  double V_gE =  6.5;
  double V_gI = -1.5;
  double tau_gE    = 0.5;
  double tau_gE_s1 = 3.0;
  double tau_gI    = 0.5;
  double tau_gI_s1 = 7.0;
  double V_threshold = 6.5;
  static const int n_var = 8;
  static const int n_var_soma = 4;  // number of variables for non- G part
  static const int id_V     = 0;    // id_V should just before gating variables(see main.cpp)
  static const int id_h     = 1;
  static const int id_m     = 2;
  static const int id_n     = 3;
  static const int id_gE    = 4;
  static const int id_gI    = 5;
  static const int id_gE_s1 = 6;
  static const int id_gI_s1 = 7;
  static const int id_gEInject = id_gE_s1;
  static const int id_gIInject = id_gI_s1;

  const double * Get_dym_default_val() const override
  {
    static const double dym_default[n_var] = {
      2.7756626542950876e-05, 5.9611104634682788e-01,
      5.2934217620863984e-02, 3.1768116757978115e-01, 0, 0, 0, 0 };
    return dym_default;
  }

  double Get_V_threshold() const override {return V_threshold;};
  int Get_id_gEInject() const override {return id_gEInject;}
  int Get_id_gIInject() const override {return id_gIInject;}
  int Get_n_dym_vars() const override {return n_var;}
  int Get_id_V() const override {return id_V;}
  int Get_id_gE() const override {return id_gE;}
  int Get_id_gI() const override {return id_gI;}
  void NextStepSingleNeuronQuiet(
    double *dym_val,
    double &t_in_refractory,
    double &spike_time_local,
    double dt_local) const override
  {  }
  void VoltHandReset(double *dym_val) const override
  {}
  void Set_Time_Refractory(double t_ref) override
  {}
};

/**
  Hawkes process "neuron". i.e. vary rate poisson process.
  The V (id_V) is cumulating "time", once the "time" reaches time threshold (id_thres),
  the neuron fires and V reset.
*/
struct Ty_Hawkes_GH: public Ty_Neuron_Dym_Base
{
  double tau_gE      = 2.0;   // ms, same as "Ty_LIF_GH_core"
  double tau_gE_s1   = 0.5;   // ms
  double tau_gI      = 5.0;   // ms
  double tau_gI_s1   = 0.8;   // ms
  double V_threshold = 0.2;   // Currently used as input current.

  static const int n_var    = 6;
  static const int id_V     = 0;
  static const int id_thres = 1;
  static const int id_gE    = 2;
  static const int id_gI    = 3;
  static const int id_gE_s1 = 4;
  static const int id_gI_s1 = 5;
  static const int id_gEInject = id_gE_s1;
  static const int id_gIInject = id_gI_s1;
  
  double Get_V_threshold() const override
  { return V_threshold; }

  int Get_id_V() const override
  { return id_V; }
  
  int Get_id_gE() const override
  { return id_gE; }

  int Get_id_gI() const override
  { return id_gI; }

  int Get_id_gEInject() const override
  { return id_gEInject; }

  int Get_id_gIInject() const override
  { return id_gIInject; }

  int Get_n_dym_vars() const override
  { return n_var; }

  void Set_Time_Refractory(double t_ref) override
  {
    /* No refractory period in this model */
  }

  inline double GetDv(const double *dym_val) const
  {
    return dym_val[id_gE] - dym_val[id_gI] + V_threshold;
  }

  MACRO_NO_INLINE double DymInplaceRK4(double *dym_val, double dt) const
  {
    /** The ODE is:
      V'(t) = GE(t) - GI(t) + I
      GE'(t) = -1/tCE  * GE(t) + HE(t)
      HE'(t) = -1/tCRE * HE(t)
      GI'(t) = -1/tCI  * GI(t) + HI(t)
      HI'(t) = -1/tCRI * HI(t)
    */

    double v_n = dym_val[id_V];
    double k1, k2, k3, k4;
    double expEC  = exp(-0.5 * dt / tau_gE);
    double expECR = exp(-0.5 * dt / tau_gE_s1);
    double expIC  = exp(-0.5 * dt / tau_gI);
    double expICR = exp(-0.5 * dt / tau_gI_s1);
    double gE_s_coef = (expEC - expECR) * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
    double gI_s_coef = (expIC - expICR) * tau_gI * tau_gI_s1 / (tau_gI - tau_gI_s1);

    // k1 = f(t_n, y_n)
    k1 = GetDv(dym_val);

    // y_n + 0.5*k1*dt
    dym_val[id_V ] = v_n + 0.5 * dt * k1;
    dym_val[id_gE] = expEC * dym_val[id_gE] + gE_s_coef * dym_val[id_gE_s1];
    dym_val[id_gI] = expIC * dym_val[id_gI] + gI_s_coef * dym_val[id_gI_s1];
    dym_val[id_gE_s1] *= expECR;
    dym_val[id_gI_s1] *= expICR;
    // k2 = f(t+dt/2, y_n + 0.5*k1*dt)
    k2 = GetDv(dym_val);

    // y_n + 0.5*k2*dt
    dym_val[id_V ] = v_n + 0.5 * dt * k2;
    // k3 = f(t+dt/2, y_n + 0.5*k2*dt)
    k3 = GetDv(dym_val);

    // y_n + k3*dt
    dym_val[id_V ] = v_n + dt * k3;
    dym_val[id_gE] = expEC * dym_val[id_gE] + gE_s_coef * dym_val[id_gE_s1];
    dym_val[id_gI] = expIC * dym_val[id_gI] + gI_s_coef * dym_val[id_gI_s1];
    dym_val[id_gE_s1] *= expECR;
    dym_val[id_gI_s1] *= expICR;
    // k4 = f(t+dt, y_n + k3*dt)
    k4 = GetDv(dym_val);

    dym_val[id_V ] = v_n + dt/6.0 * (k1 + 2*k2 + 2*k3 + k4);

    return k1;
  }

  void NextStepSingleNeuronQuiet(
    double *dym_val,
    double &t_in_refractory,
    double &spike_time_local,
    double dt_local) const override
  {
    double dym_t[n_var];
    std::copy(dym_val, dym_val+n_var, dym_t);
    double v_n = dym_val[id_V];

    double k1 = DymInplaceRK4(dym_val, dt_local);
    
    if (v_n <= dym_val[id_thres]
        && dym_val[id_V] > dym_val[id_thres]) {
      // spiked in interval [0 dt_local], different from [0 dt_local).
      spike_time_local = cubic_hermit_real_root(dt_local,
        v_n, dym_val[id_V],
        k1, GetDv(dym_val), dym_val[id_thres]);
      // Evolve to spike_time_local.
      std::copy(dym_t, dym_t+n_var, dym_val);
      DymInplaceRK4(dym_val, spike_time_local);
      dym_val[id_V    ] = 0;
      // Generate next threshold.
      static std::exponential_distribution<double> exp_dis;
      dym_val[id_thres] = 1.0 * exp_dis(rand_eng);  // TODO: use a standalone rng.
      // evolve to dt_local
      NextStepSingleNeuronQuiet(dym_val, t_in_refractory,
        spike_time_local, dt_local - spike_time_local);
    }
  }
  
  void VoltHandReset(double *dym_val) const override
  {
    dym_val[id_V] = 0;
  }

  const double * Get_dym_default_val() const override
  {
    static double dym[n_var] = {0};
    static std::exponential_distribution<double> exp_dis;
    dym[id_thres] = V_threshold * exp_dis(rand_eng);
    return dym;
  }
};

//
struct Ty_LIF_GH_single_dendritic_core
{
  // The neuron model named DIF-single-GH in this code.
  // This is the Leaky Integrate-and-Fire model with order 1 smoothed conductance. and the conductance has second order term.
  double V_threshold  = 1.0;  // voltages are in dimensionless unit
  double V_reset      = 0.0;
  double V_leakage    = 0.0;
  double V_excitatory = 14.0/3.0;
  double V_inhibitory = -2.0/3.0;
  double G_leak       = 0.05;  // ms^-1
  double synaptic_alpha = -0.01;												// EDIT YWS: snaptic alpha is usually negative
  double tau_gE       = 2.0;   // ms
  double tau_gE_s1    = 0.5;   // ms
  double tau_gI       = 5.0;   // ms
  double tau_gI_s1    = 0.8;   // ms
  double Time_Refractory = 2.0;   // ms
  static const int n_var    = 5;  // number of dynamical variables
  static const int id_V     = 0;  // index of V variable
  static const int id_gE    = 1;  // index of gE variable
  static const int id_gI    = 2;  // index of gI variable
  static const int id_gE_s1 = 3;  // index of derivative of gE
  static const int id_gI_s1 = 4;  // index of derivative of gI
  static const int id_gEInject = id_gE_s1;  // index of gE injection variable
  static const int id_gIInject = id_gI_s1;  // index of gI injection variable

  // Evolve conductance only
  void NextDtConductance(double *dym_val, double dt) const
  {
    // Excitatory
    double expC  = exp(-dt / tau_gE);
    double expCR = exp(-dt / tau_gE_s1);
    dym_val[id_gE] = expC*dym_val[id_gE] + (expC - expCR) * dym_val[id_gE_s1] * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
    dym_val[id_gE_s1] *= expCR;
    // Inhibitory
    expC  = exp(-dt / tau_gI);
    expCR = exp(-dt / tau_gI_s1);
    dym_val[id_gI] = expC*dym_val[id_gI] + (expC - expCR) * dym_val[id_gI_s1] * tau_gI * tau_gI_s1 / (tau_gI - tau_gI_s1);
    dym_val[id_gI_s1] *= expCR;
  }

  // Get instantaneous dv/dt for current dynamical state
  inline double GetDv(const double *dym_val) const
  {
    return - G_leak * (dym_val[id_V] - V_leakage)
           - dym_val[id_gE] * (dym_val[id_V] - V_excitatory)
           - dym_val[id_gI] * (dym_val[id_V] - V_inhibitory)
           - synaptic_alpha * dym_val[id_gE] * dym_val[id_gI] * // EDIT YWS: if we set synaptic alpha to be nagative, the sign here shall be negative
             ((dym_val[id_V] - V_excitatory));					
  }

  // Evolve the state `dym_val' a `dt' forward,
  // using classical Runge–Kutta 4-th order scheme for voltage.
  // Conductance will evolve using the exact formula.
  // Return derivative k1 at t_n, for later interpolation.
  MACRO_NO_INLINE double DymInplaceRK4(double *dym_val, double dt) const
  {
    double v_n = dym_val[id_V];
    double k1, k2, k3, k4;
    double expEC  = exp(-0.5 * dt / tau_gE);  // TODO: maybe cache this value?
    double expECR = exp(-0.5 * dt / tau_gE_s1);
    double expIC  = exp(-0.5 * dt / tau_gI);
    double expICR = exp(-0.5 * dt / tau_gI_s1);
    double gE_s_coef = (expEC - expECR) * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
    double gI_s_coef = (expIC - expICR) * tau_gI * tau_gI_s1 / (tau_gI - tau_gI_s1);

    // k1 = f(t_n, y_n)
    k1 = GetDv(dym_val);

    // y_n + 0.5*k1*dt
    dym_val[id_V ] = v_n + 0.5 * dt * k1;
    dym_val[id_gE] = expEC * dym_val[id_gE] + gE_s_coef * dym_val[id_gE_s1];
    dym_val[id_gI] = expIC * dym_val[id_gI] + gI_s_coef * dym_val[id_gI_s1];
    dym_val[id_gE_s1] *= expECR;
    dym_val[id_gI_s1] *= expICR;
    // k2 = f(t+dt/2, y_n + 0.5*k1*dt)
    k2 = GetDv(dym_val);

    // y_n + 0.5*k2*dt
    dym_val[id_V ] = v_n + 0.5 * dt * k2;
    // k3 = f(t+dt/2, y_n + 0.5*k2*dt)
    k3 = GetDv(dym_val);

    // y_n + k3*dt
    dym_val[id_V ] = v_n + dt * k3;
    dym_val[id_gE] = expEC * dym_val[id_gE] + gE_s_coef * dym_val[id_gE_s1];
    dym_val[id_gI] = expIC * dym_val[id_gI] + gI_s_coef * dym_val[id_gI_s1];
    dym_val[id_gE_s1] *= expECR;
    dym_val[id_gI_s1] *= expICR;
    // k4 = f(t+dt, y_n + k3*dt)
    k4 = GetDv(dym_val);

    dym_val[id_V ] = v_n + dt/6.0 * (k1 + 2*k2 + 2*k3 + k4);

    return k1;
  }
};

typedef Ty_LIF_stepper<Ty_LIF_GH_single_dendritic_core> Ty_DIF_single_GH;

/////////////////////////////////////////////////////////////////////////////
// Model of HH-GH with bilinear dentritic interaction

struct Ty_DIF_GH_core
{
	// The neuron model named DIF-GH in this code.
	// This is the Leaky Integrate-and-Fire model with order 1 smoothed conductance.
	double V_threshold = 1.0;  // voltages are in dimensionless unit
	double V_reset = 0.0;
	double V_leakage = 0.0;
	double V_excitatory = 14.0 / 3.0;
	double V_inhibitory = -2.0 / 3.0;
	double G_leak = 0.05;  // ms^-1
	double tau_gE = 2.0;   // ms
	double tau_gE_s1 = 0.5;   // ms
	double tau_gI = 5.0;   // ms
	double tau_gI_s1 = 0.8;   // ms
	double Time_Refractory = 2.0;   // ms  

	TyMatVals* alpha;			 // coeffcients for dentritic interaction
								 // it will be initialized in NeuronPopulationDendriticDeltaInteractTemplate

	int n_neu;			 // number of neurons, it will be intialized in 
					     // NeuronPopulationDendriticDeltaInteractTemplate and other similar classes


	/* The following costants describes how the dynmaic varibles are stored in the memory 
	 * for a single neuron, voltage, conductances, derivatives of conductances are stored continously
	 * the order is:
	 * V gEP gIP gEPs gIPs  gE(1) gE(2) ... gE(n)  gI(1) gI(2) ... gI(n)  gEs(1) gEs(2) ... gEs(n)  gIs(1) gIs(2) ... gIs(n)
	 * where:
	 * V is the voltage of the neuron, 
	 * gEP (gIP) is the excitatory(inhibitory) conductance related with poisson input
	 * gE(i) (gI(i)) is the excitatory(inhibitory) conductance input from the ith neuron, n is the number of ALL neurons
	 * gEPs, gIPs, gEs, gIs are the corresponding derivatives
	 *
	 * @@get_id_gE and others will help when deal with a specific conductance
	 */

	static const int n_var = 5;  // number of dynamical variables, EXculding gE(i), gI(i), gEs(i), gIs(i)
	static const int id_V = 0;   // index of V variable
	static const int id_gEPoisson = 1; // index of conductance rlatinng to Poisson input
	static const int id_gIPoisson = 2; // index of conductance rlatinng to Poisson input
	static const int id_gEPoisson_s1 = 3; // index of conductance rlatinng to Poisson input
	static const int id_gIPoisson_s1 = 4; // index of conductance rlatinng to Poisson input

	// The following id shall be interpreted differentyl, since the there are a vector of gE, gI, etc
	// id_x = n really means the data is stored continously in dymvals 
	// from n_var + id_x * n_neu to n_var + (id_x + 1) * neu
	static const int id_gE = 0;  // index bais coeff of gE variable
	static const int id_gI = 1;  // index bias coeff of gI variable
	static const int id_gE_s1 = 2;  // index bias coeff of derivative of gE
	static const int id_gI_s1 = 3;  // index bias coeff of derivative of gI
	static const int id_gEInject = id_gE_s1;  // index of bias coeff gE injection variable
	static const int id_gIInject = id_gI_s1;  // index of bias coeff gI injection variable

	// the real index of gE(etc) of the i-th neuron
	inline int get_id_gE(int neuron_id) const { return n_var + n_neu * id_gE + neuron_id; }			//neuron_id count starting from 0
	inline int get_id_gI(int neuron_id) const { return n_var + n_neu * id_gI + neuron_id; }
	inline int get_id_gE_s1(int neuron_id) const { return n_var + n_neu * id_gE_s1 + neuron_id; }
	inline int get_id_gI_s1(int neuron_id) const { return n_var + n_neu * id_gI_s1 + neuron_id; }

											  // Evolve conductance only
	void NextDtConductance(double *dym_val, double dt) const
	{
		/**
		ODE:
		g '[t] == -1/tC  * g [t] + gR[t]
		gR'[t] == -1/tCR * gR[t]
		Solution:
		g [t] = exp(-t/tC) * g[0] + (exp(-t/tC) - exp(-t/tCR)) * gR[0] * tC * tCR / (tC - tCR)
		gR[t] = exp(-t/tCR) * gR[0]
		Another form (hopefully more accurate, note exp(x)-1 is expm1(x) ):
		g [t] = exp(-t/tC) * (g[0] + gR[0] * (exp((1/tC-1/tCR)*t) - 1) / (1/tC - 1/tCR))
		Or
		g [t] = exp(-t/tC) * g[0] + exp(-t/tCR) * gR[0] * (exp((1/tCR-1/tC)*t) - 1) / (1/tCR-1/tC)
		*/
		double expCE = exp(-dt / tau_gE);
		double expCRE = exp(-dt / tau_gE_s1);

		double expCI = exp(-dt / tau_gI);
		double expCRI = exp(-dt / tau_gI_s1);

		// The two lines below is for test, based on my guess YWS, it seems works well 
		dym_val[id_gEPoisson] = expCE*dym_val[id_gEPoisson] + (expCE - expCRE) * dym_val[id_gEPoisson_s1] * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
		dym_val[id_gIPoisson] = expCI*dym_val[id_gIPoisson] + (expCI - expCRI) * dym_val[id_gIPoisson_s1] * tau_gI * tau_gI_s1 / (tau_gI - tau_gI_s1);
		dym_val[id_gEPoisson_s1] *= expCRE;		// FIXED 2018/09/18 YWS Note that the correction may not be correct
		dym_val[id_gIPoisson_s1] *= expCRI;
		
		for (int i = 0; i < n_neu; i++) {
			// exhibitory
			dym_val[get_id_gE(i)] = expCE*dym_val[get_id_gE(i)] + (expCE - expCRE) * dym_val[get_id_gE_s1(i)] * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
			dym_val[get_id_gE_s1(i)] *= expCRE;
			// inhibitory
			dym_val[get_id_gI(i)] = expCI*dym_val[get_id_gI(i)] + (expCI - expCRI) * dym_val[get_id_gI_s1(i)] * tau_gI * tau_gI_s1 / (tau_gI - tau_gI_s1);
			dym_val[get_id_gI_s1(i)] *= expCRI;
		}
		
	}

	// Get instantaneous dv/dt for current dynamical state
	inline double GetDv(const double *dym_val) const
	{
		double v_n = dym_val[id_V];
		double v_E_diff = v_n - V_excitatory;
		double v_I_diff = v_n - V_inhibitory;

		double retval = 0;
		retval += G_leak * (v_n - V_leakage);
		retval += dym_val[id_gEPoisson] * v_E_diff;
		retval += dym_val[id_gIPoisson] * v_I_diff;

		
		for (int i = 0; i < n_neu; i++) {
			retval += dym_val[get_id_gE(i)] * v_E_diff;
			retval += dym_val[get_id_gI(i)] * v_I_diff;

		}
		for (int i = 0; i < n_neu; i++) {
			for (int j = 0; j < n_neu; j++) {
				retval += alpha->at(i).at(j) * dym_val[get_id_gE(i)] * dym_val[get_id_gI(j)] * v_E_diff; // bug FIXED 11/12 YWS
			}
		}

		retval += alpha->at(0).at(0) * dym_val[id_gEPoisson] * dym_val[id_gIPoisson] * v_E_diff;
		

		return -retval;  // NOTE that in fact all terms are negative
	}

	// Evolve the state `dym_val' a `dt' forward,
	// using classical Runge–Kutta 4-th order scheme for voltage.
	// Conductance will evolve using the exact formula.
	// Return derivative k1 at t_n, for later interpolation.
	MACRO_NO_INLINE double DymInplaceRK4(double *dym_val, double dt) const
	{

		double v_n = dym_val[id_V];
		double k1, k2, k3, k4;
		double expEC = exp(-0.5 * dt / tau_gE);  // TODO: maybe cache this value?
		double expECR = exp(-0.5 * dt / tau_gE_s1);
		double expIC = exp(-0.5 * dt / tau_gI);
		double expICR = exp(-0.5 * dt / tau_gI_s1);
		double gE_s_coef = (expEC - expECR) * tau_gE * tau_gE_s1 / (tau_gE - tau_gE_s1);
		double gI_s_coef = (expIC - expICR) * tau_gI * tau_gI_s1 / (tau_gI - tau_gI_s1);

		// k1 = f(t_n, y_n)
		k1 = GetDv(dym_val);

		// y_n + 0.5*k1*dt
		dym_val[id_V] = v_n + 0.5 * dt * k1;

		dym_val[id_gEPoisson] = expEC * dym_val[id_gEPoisson] + gE_s_coef * dym_val[id_gEPoisson_s1];
		dym_val[id_gIPoisson] = expIC * dym_val[id_gIPoisson] + gI_s_coef * dym_val[id_gIPoisson_s1];
		dym_val[id_gEPoisson_s1] *= expECR;
		dym_val[id_gIPoisson_s1] *= expICR;
		for (int i = 0; i < n_neu; i++) {
			dym_val[get_id_gE(i)] = expEC * dym_val[get_id_gE(i)] + gE_s_coef * dym_val[get_id_gE_s1(i)];
			dym_val[get_id_gI(i)] = expIC * dym_val[get_id_gI(i)] + gI_s_coef * dym_val[get_id_gI_s1(i)];
			dym_val[get_id_gE_s1(i)] *= expECR;
			dym_val[get_id_gI_s1(i)] *= expICR;
		}
		// k2 = f(t+dt/2, y_n + 0.5*k1*dt)
		k2 = GetDv(dym_val);

		// y_n + 0.5*k2*dt
		dym_val[id_V] = v_n + 0.5 * dt * k2;
		// k3 = f(t+dt/2, y_n + 0.5*k2*dt)
		k3 = GetDv(dym_val);

		// y_n + k3*dt
		dym_val[id_V] = v_n + dt * k3;
		dym_val[id_gEPoisson] = expEC * dym_val[id_gEPoisson] + gE_s_coef * dym_val[id_gEPoisson_s1];
		dym_val[id_gIPoisson] = expIC * dym_val[id_gIPoisson] + gI_s_coef * dym_val[id_gIPoisson_s1];
		dym_val[id_gEPoisson_s1] *= expECR;
		dym_val[id_gIPoisson_s1] *= expICR;
		for (int i = 0; i < n_neu; i++) {
			dym_val[get_id_gE(i)] = expEC * dym_val[get_id_gE(i)] + gE_s_coef * dym_val[get_id_gE_s1(i)];
			dym_val[get_id_gI(i)] = expIC * dym_val[get_id_gI(i)] + gI_s_coef * dym_val[get_id_gI_s1(i)];
			dym_val[get_id_gE_s1(i)] *= expECR;
			dym_val[get_id_gI_s1(i)] *= expICR;
		}
		// k4 = f(t+dt, y_n + k3*dt)
		k4 = GetDv(dym_val);

		dym_val[id_V] = v_n + dt / 6.0 * (k1 + 2 * k2 + 2 * k3 + k4);

		return k1;
	}
};

// Adapter for DIF model (only sub-threshold dynamics and need hand reset)
template<typename TyNeuronModel>
struct Ty_DIF_stepper : public TyNeuronModel, public Ty_Neuron_Dym_Base
{
	// for template class we need these "using"s. It's a requirement for TyNeuronModel.
	using TyNeuronModel::id_V;	
	using TyNeuronModel::id_gE;	// not to use to avoid confusion, interpreted in DIF-GH, DO READ the comments there before use them
	using TyNeuronModel::id_gI; // use with care if needed, READ the comments!
	using TyNeuronModel::V_threshold;
	using TyNeuronModel::V_reset;
	using TyNeuronModel::Time_Refractory;
	using TyNeuronModel::GetDv;
	using TyNeuronModel::NextDtConductance;
	using TyNeuronModel::DymInplaceRK4;

	using TyNeuronModel::id_gEInject;
	using TyNeuronModel::id_gIInject;
	using TyNeuronModel::n_var;
	// YWS the following is added for DIF
	using TyNeuronModel::n_neu;
	using TyNeuronModel::id_gEPoisson_s1; // not used so far
	using TyNeuronModel::id_gIPoisson_s1;


	double Get_V_threshold() const override { return V_threshold; };
	int Get_id_gEInject() const override { return id_gEInject; assert(0); } // These functions shall not be called to avoid confusion
	int Get_id_gIInject() const override { return id_gIInject; assert(0); } // use with care if u do need them
	int Get_n_dym_vars() const override { return n_var; }   // OK to use
	int Get_id_V() const override { return id_V; }			// OK to use
	int Get_id_gE() const override { return id_gE; assert(0);}
	int Get_id_gI() const override { return id_gI; assert(0);}

	void Set_Time_Refractory(double t_ref) override
	{
		if (t_ref >= 0) {
			Time_Refractory = t_ref;  // No error checking
		}
		else {
			cerr << "Setting Time_Refractory < 0 : t_ref = " << t_ref << "\n";
		}
	}

	// Used when reset the voltage by hand. (e.g. outside this class)
	inline void VoltHandReset(double *dym_val) const override
	{
		dym_val[id_V] = V_reset;
	}

	// in fact, we never use it(? YWSQ )
	void SpikeTimeRefine(double *dym_t, double &t_spike, int n_it, int n_E, int n_I) const
	{
		if (n_it == 1) {
			DymInplaceRK4(dym_t, t_spike);
			t_spike -= (dym_t[id_V] - V_threshold) / GetDv(dym_t);
		}
		else {  // multiple iterations
			double dym_t0[n_var];
			std::copy(dym_t, dym_t + n_var, dym_t0);  // copy of init state
			for (int i = n_it - 1; i >= 0; i--) {
				DymInplaceRK4(dym_t, t_spike);
				/*t_spike -= (dym_t[id_V]-V_threshold) / GetDv(dym_t);*/
				double dt_spike = (dym_t[id_V] - V_threshold) / GetDv(dym_t);
				t_spike -= dt_spike;
				dbg_printf("SpikeTimeRefine(): dt = %.17g, t_spike = %.17g, V = %.17g, Dv = %g\n",
					dt_spike, t_spike, dym_t[id_V], GetDv(dym_t));
				if (i) std::copy(dym_t0, dym_t0 + n_var, dym_t);
			}
		}
	}

	// Evolve the ODE and note down the spike time, assuming no reset and no external input.
	// `spike_time_local' should be guaranteed to be within [0, dt] or NAN.
	MACRO_NO_INLINE void NextStepSingleNeuronContinuous(double *dym_val, double &spike_time_local, double dt) const
	{
		double dym_t[n_var];
		std::copy(dym_val, dym_val + n_var, dym_t);
		double v_n = dym_val[id_V];						// the original V
		double k1 = DymInplaceRK4(dym_val, dt);

		// if there is a spike in this dt
		if (v_n <= V_threshold							// the original V
			&& dym_val[id_V] > V_threshold) {			// V, after evolve for dt
			spike_time_local = cubic_hermit_real_root(dt,
				v_n, dym_val[id_V],
				k1, GetDv(dym_val), V_threshold);
			// refine spike time
			/*SpikeTimeRefine(dym_t, spike_time_local, 2);*/
		}
		else {
			if (v_n > 0.996 && k1>0) { // the v_n > 0.996 is for dt=0.5 ms, LIF,G model
									   // Try capture some missing spikes that the intermediate value passes
									   // threshold, but both ends are lower than threshold.
									   // Get quadratic curve from value of both ends and derivative from left end
									   // Return the maximum point as `t_max_guess'
				double c = v_n;
				double b = k1;
				double a = (dym_val[id_V] - c - b*dt) / (dt*dt);
				double t_max_guess = -b / (2 * a);
				// In LIF-G, it can guarantee that a<0 (concave),
				// hence t_max_guess > 0. But in LIF-G model, we still need to
				// check 0 < t_max_guess
				if (0 < t_max_guess && t_max_guess < dt
					&& (b*b) / (-4 * a) + c >= V_threshold) {
					//dbg_printf("Rare event: mid-dt spike captured, guess time: %f\n", t_max_guess);
					dbg_printf("NextStepSingleNeuronContinuous(): possible mid-dt spike detected:\n");
					dbg_printf("  Guessed max time: %f, dt = %f\n", t_max_guess, dt);
					// root should in [0, t_max_guess]
					spike_time_local = cubic_hermit_real_root(dt,
						v_n, dym_val[id_V],
						k1, GetDv(dym_val), V_threshold);
					/*SpikeTimeRefine(dym_t, spike_time_local, 2);*/
				}
				else {
					spike_time_local = std::numeric_limits<double>::quiet_NaN();
				}
			}
			else {
				spike_time_local = std::numeric_limits<double>::quiet_NaN();
			}
		}
	}

	// Evolve single neuron as if no external input.
	// Return first spike time in `spike_time_local', if any.
	void NextStepSingleNeuronQuiet(double *dym_val, double &t_in_refractory,
		double &spike_time_local, double dt_local) const override
	{
		//! at most one spike allowed during this dt_local
		if (t_in_refractory == 0) {
			dbg_printf("NextStepSingleNeuronQuiet(): dt_local = %.17g\n", dt_local);
			dbg_printf("NextStepSingleNeuronQuiet(): begin state=%.16e,%.16e,%.16e\n",
				dym_val[0], dym_val[1], dym_val[2]);
			NextStepSingleNeuronContinuous(dym_val, spike_time_local, dt_local);
			dbg_printf("NextStepSingleNeuronQuiet(): end   state=%.16e,%.16e,%.16e\n",
				dym_val[0], dym_val[1], dym_val[2]);
			if (!std::isnan(spike_time_local)) {
				// Add `numeric_limits<double>::min()' to make sure t_in_refractory > 0.
				t_in_refractory = dt_local - spike_time_local
					+ std::numeric_limits<double>::min();
				dym_val[id_V] = V_reset;
				dbg_printf("NextStepSingleNeuronQuiet(): neuron fired, spike_time_local = %.17g\n", spike_time_local);
				if (t_in_refractory >= Time_Refractory) {
					// Short refractory period (< dt_local), neuron will be actived again.
					dt_local = t_in_refractory - Time_Refractory;
					t_in_refractory = 0;
					// Back to the activation time.
					NextDtConductance(dym_val, -dt_local);
					double spike_time_local_tmp;
					NextStepSingleNeuronContinuous(dym_val, spike_time_local_tmp, dt_local);
					if (!std::isnan(spike_time_local_tmp)) {
						cerr << "NextStepSingleNeuronQuiet(): Multiple spikes in one dt. Interaction dropped." << endl;
						cerr << "  dt_local = " << dt_local << '\n';
						cerr << "  spike_time_local = " << spike_time_local << '\n';
						cerr << "  t_in_refractory = " << t_in_refractory << '\n';
						cerr << "  dym_val[id_V] = " << dym_val[id_V] << '\n';
						cerr << "  dym_val[id_gE] = " << dym_val[id_gE] << '\n';
						throw "Multiple spikes in one dt.";
					}
				}
			}
		}
		else {
			// Neuron in refractory period.
			double dt_refractory_remain = Time_Refractory
				- t_in_refractory;
			if (dt_refractory_remain < dt_local) {
				// neuron will awake after dt_refractory_remain which is in this dt_local
				NextDtConductance(dym_val, dt_refractory_remain);
				assert(dym_val[id_V] == V_reset);
				t_in_refractory = 0;
				NextStepSingleNeuronQuiet(dym_val, t_in_refractory,
					spike_time_local, dt_local - dt_refractory_remain);
			}
			else {
				spike_time_local = std::numeric_limits<double>::quiet_NaN();
				NextDtConductance(dym_val, dt_local);
				t_in_refractory += dt_local;
			}
		}
	}

	const double * Get_dym_default_val() const
	{
		static const double dym[n_var] = { 0 };
		return dym;
	}
};

typedef Ty_DIF_stepper<Ty_DIF_GH_core> Ty_DIF_GH;


#endif
