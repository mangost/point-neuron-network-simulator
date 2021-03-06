% Convergence across different simulator
addpath('../mfile');
maxabs = @(x) max(abs(x(:)));

% single neuron case

%pm = [];
%pm.t    = 1e3;
%pm.dt   = 1/32;
%pm.stv  = 1/32;
%pm.pr   = 1.0;
%pm.ps   = 0.04;
%pm.seed = 123; % randi(2^31-1, 1, 1);
%pm.neuron_model = 'HH-GH-cont-syn';
%pm.simu_method  = 'auto';
%pm.spike_threshold = 1.0;
%pm.input_event = ie;
%pm.extra_cmd = '';

% network case

pm0 = [];
pm0.neuron_model = 'LIF-GH';
pm0.net  = ones(15);
pm0.nI   = 0;
pm0.scee = 0.05;
pm0.scie = 0.06;
pm0.scei = 0.07;
pm0.scii = 0.04;
pm0.pr   = 1.0;
pm0.ps   = 0.02;
pm0.t    = 64;
pm0.dt   = 1.0/32;
%pm0.stv  = pm0.dt;
pm0.stv  = 1.0/32;
pm0.seed = 4563;

n_points = 8;
s_dt = zeros(n_points, 1);
dt0 = 1.0/32;
for i = 1:n_points
    s_dt(i) = dt0 / 2^i;
end
    
%s_dt = 1 ./ [1024 2048 4096 8192]; % 2048 4096 8192 %[32 64 128 256 512 1024]
s_err_V    = zeros(size(s_dt));
s_err_Vend = zeros(size(s_dt));
s_err_ras  = zeros(size(s_dt));

pm = pm0;

% Reference answer
pm.dt = s_dt(end)/4.0;
[X_r, isi_r, ras_r, pm_expand_r] = gen_neu(pm, 'new,rm,verbose');

v0 = X_r(:, end);
t_last0 = lastRASEvent(ras_r, size(pm.net,1));
v0(pm.t - t_last0 < 4 | v0 > 10 | v0 < 0) = nan;

for id_dt = 1:numel(s_dt)
  pm.dt = s_dt(id_dt);
  [X, isi, ras, pm_expand] = gen_neu(pm, 'new,rm,verbose');

  v = X(:, end);
  t_last = lastRASEvent(ras, size(pm.net,1));
  v(pm.t - t_last < 4 | v > 10 | v < 0) = nan;

  s_err_Vend(id_dt) = maxabs(v - v0);
  s_err_V(id_dt) = maxabs(X - X_r) / maxabs(X_r);
  s_err_ras(id_dt) = maxabs(t_last - t_last0);
end

figure(1);
semilogy(s_dt, s_err_V, '-o');
ylabel('V err');
xlabel('dt (ms)');

figure(2);
semilogy(s_dt, s_err_ras, '-o');
ylabel('ras err');
xlabel('dt (ms)');

return

pick_ras_local = @(ras, rg) ras(rg(1)*pm0.stv<ras(:,2) & ras(:,2)<rg(end)*pm0.stv, :);

rg = 1:100000;
figure(21);
ras_local = pick_ras_local(ras, rg);
ras_l_local = pick_ras_local(ras_l, rg);
plot(rg, X(:, rg),...
     rg, X_l(:, rg),...
     rg, 10*SpikeTrains(ras_local, size(X,1), length(rg), pm0.stv),...
     rg, 10*SpikeTrains(ras_l_local, size(X_l,1), length(rg), pm0.stv)...
     );

figure(21);
plot(X(rg) - X_l(rg))

