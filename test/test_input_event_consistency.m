% Test consistency across versions
fprintf('~~~~~~ Consistency Test Across Version ~~~~~~\n');

addpath('../mfile');
maxabs = @(x) max(abs(x(:)));
old_rand_state = randMT19937('state');
randMT19937('state', 12353512);

s_neuron_model = {'LIF-G', 'LIF-GH', 'HH-G', 'HH-GH', 'HH-GH-cont-syn', 'IF-jump'};
path_ref_executable = '../bin/gen_neu_c5dfc0be';
path_tag_executable = '../bin/gen_neu';

for id_nm = 1 %:length(s_neuron_model)
    fprintf('====== Testing model: %s ======\n', s_neuron_model{id_nm});
    network = gen_net_er(15, 0.5, floor(1e9*randMT19937(1)));
    
    pm = [];
    pm.prog_path = path_ref_executable;
    pm.neuron_model = s_neuron_model{id_nm};
    pm.simu_method  = 'auto';
%    pm.synaptic_delay = 0.5;
%    pm.simu_method  = 'SSC';
    pm.net     = network;
    pm.nI      = 5;
    pm.scee_mV = 1.0;
    pm.scie_mV = 1.2;
    pm.scei_mV = 1.4;
    pm.scii_mV = 1.8;
    pm.pr      = 2.0 * (1+randMT19937(1,length(pm.net))/10);
    pm.ps_mV   = 1.0 * (1+randMT19937(1,length(pm.net))/10);
    pm.pri     = 2.0 * (1+randMT19937(1,length(pm.net))/10);
    pm.psi_mV  = 0.7 * (1+randMT19937(1,length(pm.net))/10);
    pm.t    = 1e4;
    pm.dt   = 1.0/32;
    pm.stv  = 0.5;
    pm.seed = 5866;
    pm.extra_cmd = '-v';
    
    % Standard data
%    [X_ref, ISI_ref, ~, pm] = gen_neu(pm, 'new,rm');
%    fprintf('  ISI = %g\n', mean(ISI_ref));
    
    % Test new version, reproduce, output of poisson events
    pm.prog_path = path_tag_executable;
    pm.extra_cmd = '-v --output-poisson-path poi.txt';
    [X, ISI, ras, pm] = gen_neu(pm, 'new,rm');
    fprintf('  ISI = %g\n', mean(ISI));
%    fprintf('--> Result: Max diff = %g\n', maxabs(X - X_ref));

    % Test reading of poisson events
    pm.extra_cmd = '-v --input-event-path poi.txt';
    [X_poi, ISI_poi, ras, pm] = gen_neu(pm, 'new,rm');
    fprintf('  ISI = %g\n', mean(ISI_poi));
    fprintf('--> Result: Max diff poi = %g\n', maxabs(X_poi - X));

    % Test exporting spike events to a neuron
    if isempty(strfind(pm.neuron_model, 'cont'))
        % construct input events to neuron "id_test".
        id_test = 1;
        % input from poisson
        tic;
        poi = load('poi.txt');
        fprintf('load poi.txt: t = %.3f\n', toc);
        poi = poi(poi(:,1)==id_test, :);  % keep only id_test related spikes
        poi(:,1) = 1;
        % input from other neurons in network
        ras = ras(ras(:,1)~=id_test, :);
        sc_strength = [pm.scee, pm.scie, -pm.scei, -pm.scii];
        ras_strength = pm.net_adj(id_test, ras(:, 1)) .* sc_strength((id_test>pm.nE) + 2*(ras(:, 1)>pm.nE) + 1);    
        ras = [ras ras_strength'];
        ras(ras(:, 3)==0, :) = [];
        ras(:, 1) = 1;
        
        if isfield(pm, 'synaptic_delay')
            ras(:, 2) += pm.synaptic_delay;
        end
        
        poi = [poi; ras];
        [~, id_sort] = sort(poi(:,2));
        poi = poi(id_sort, :);
        
        fd = fopen('external_event.txt', 'w');
        fprintf(fd, '%d %.16e %.16e\n', poi');
        fclose(fd);

        pm.input_event = poi;
        pm.extra_cmd = '-v';
        pm.net     = zeros(1);
        pm.nE      = 1;
        pm.nI      = 0;
        [X_single, ~, ~, pm] = gen_neu(pm, 'new,rm');

        % The error is generally not zero, but will converge to zero when
        % dt -> 0. The problem is that the Spike-Correction algorithm (SSC)
        % sub-divides the time intervals more than necessary.
        fprintf('--> Result: Max diff single = %g\n', maxabs(X(1,:) - X_single));
    end
end

randMT19937('state', old_rand_state);

return

%%plot(X, X_ref)
rg = 1:200;
figure(1);
plot(X(1, rg) - X_single(1, rg))

figure(2);
plot(X(1, rg))

% vim: et sw=4 sts=4
