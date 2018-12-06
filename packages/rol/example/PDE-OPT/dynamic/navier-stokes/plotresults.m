
adj   = load('cell_to_node_quad.txt') + 1;       %% load node adjacency, increment by 1 for 1-based indexing
nodes = load('nodes.txt');                       %% load node coordinates
N     = 3*length(nodes);                         %% determine number of nodes
nt    = numel(dir('uncontrolled_state.*.txt'));  %% determine number of time steps

%% Read in state
Ux = cell(nt,1);
Uy = cell(nt,1);
Um = cell(nt,1);
P  = cell(nt,1);
for i = 0:nt-1
  data_obj  = importdata(['uncontrolled_state.',int2str(i),'.txt'], ' ', 2);  %% we need to skip the first two lines
  state     = data_obj.data;
  data_obj  = importdata(['map_uncontrolled_state.',int2str(i),'.txt'], ' ', 9);  %% we need to skip the first 9 lines
  map_state = data_obj.data;
  map_state = map_state(1:2:end)+1;
  [tmp, state_permute] = sort(map_state);
  state = state(state_permute);  %% we need to permute the state according to parallel maps

  Ux{i+1} = state(1:3:N);  %% Extract x-velocity
  Uy{i+1} = state(2:3:N);  %% Extract y-velocity
  Um{i+1} = sqrt(Ux{i+1}.^2+Uy{i+1}.^2);
  P{i+1}  = state(3:3:N);  %% Extract pressure
end

minUx = min(min([Ux{:}])); maxUx = max(max([Ux{:}]));
minUy = min(min([Uy{:}])); maxUy = max(max([Uy{:}]));
minUm = min(min([Um{:}])); maxUm = max(max([Um{:}]));
minP  = min(min([P{:}]));  maxP  = max(max([P{:}]));

axsize = 400;
figure('Position', [100 100 4*axsize 4*axsize]);
for i=1:nt-1
  subplot(3,2,1)
  trisurf(adj, nodes(:,1), nodes(:,2), Ux{i+1});
  shading interp;
  view(0,90)
  axis('equal','tight');
  xlabel('x');
  ylabel('y');
  caxis([minUx,maxUx])
  title('x-Velocity','fontsize',16);
  set(gca, 'FontSize', 16); set(gcf, 'Color', 'White');% tightfig;

  subplot(3,2,3)
  trisurf(adj, nodes(:,1), nodes(:,2), Uy{i+1});
  shading interp;
  view(0,90)
  axis('equal','tight');
  xlabel('x');
  ylabel('y');
  caxis([minUy,maxUy])
  title('y-Velocity','fontsize',16);
  set(gca, 'FontSize', 16); set(gcf, 'Color', 'White');% tightfig;

  subplot(3,2,2)
  trisurf(adj, nodes(:,1), nodes(:,2), Um{i+1});
  shading interp;
  view(0,90)
  axis('equal','tight');
  xlabel('x');
  ylabel('y');
  caxis([minUm,maxUm])
  title('Velocity Magnitude','fontsize',16);
  set(gca, 'FontSize', 16); set(gcf, 'Color', 'White');% tightfig;

  subplot(3,2,4)
  trisurf(adj, nodes(:,1), nodes(:,2), P{i+1});
  shading interp;
  view(0,90)
  axis('equal','tight');
  xlabel('x');
  ylabel('y');
  %caxis([minP,maxP])
  title('Pressure','fontsize',16);
  set(gca, 'FontSize', 16); set(gcf, 'Color', 'White');% tightfig;

  subplot(3,2,[5 6])
  quiver(nodes(:,1), nodes(:,2), Ux{i+1}, Uy{i+1});
  axis('equal','tight');
  xlabel('x');
  ylabel('y');
  title(['Time/T = ',num2str(i/(nt-1))],'fontsize',16);
  set(gca, 'FontSize', 16); set(gcf, 'Color', 'White');% tightfig;

  drawnow
end

%figure('Position', [100 100 4*axsize 4*axsize]);
%for i=0:nt-1
%  data_obj = importdata(['state.',int2str(i),'.txt'], ' ', 2);  %% we need to skip the first two lines
%  state    = data_obj.data;
%
%  Ux = state(1:3:N);  %% Extract x-velocity
%  Uy = state(2:3:N);  %% Extract y-velocity
%  P  = state(3:3:N);  %% Extract pressure
%
%  quiver(nodes(:,1), nodes(:,2), Ux, Uy);
%  axis('equal','tight');
%  title(['Time t = ',num2str(i/(nt-1))],'fontsize',16);
%  xlabel('x');
%  ylabel('y');
%  set(gca, 'FontSize', 16); set(gcf, 'Color', 'White');% tightfig;
%  drawnow
%end
