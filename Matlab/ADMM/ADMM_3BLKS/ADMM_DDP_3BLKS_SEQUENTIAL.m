function [x, u, cost] = ADMM_DDP_3BLKS_SEQUENTIAL(DYNCST, x0, u0, Op)
%---------------------- user-adjustable parameters ------------------------
global RC K x_des 
% --- initial sizes and controls
n   = size(x0,1);          % dimension of state vector
m   = size(u0, 1);          % dimension of control vector
N   = size(u0, 2);          % number of state transitions
u   = u0;                   % initial control sequence


% --- initialize trace data structure
% trace = struct('iter',nan,'cost',nan,...
%         'dcost',nan);
% trace = repmat(trace,[min(Op.maxIter,1e6) 1]);
% trace(1).iter = 1;
iter = 5;

        
% --- initial trajectory
x_init = zeros(n,N+1);
u_init = zeros(m,N+1);
c_init = zeros(1,N+1);

[x,un,~]   = traj_sim(x0, u0, DYNCST,zeros(1,5),x_init,c_init,u_init,zeros(7,N+1),zeros(7,N+1));
% [x0b,u0b,~]  = initialtraj(x0,u0_bar,DYNCST);
u          = un;
for j = 1:size(x_init,2)
    J         = Jac_kuka(x_init(1:7, j)); % jacobian at the base of the manipulator
    x_dot     = J * x_init(8:14, j);
    c(j)      = 0.3 * sum(x_dot(1:3).^2, 1) ./ RC(j);
end
% c          = 0.3 * sum(x(15:17,:).^2,1) ./ 10000;

% user plotting
% Op.plotFn(x);

% constants, timers, counters
stop        = 0;
dcost       = 0;
print_head  = 6; % print headings every print_head lines
last_head   = print_head;
verbosity   = 1;

if verbosity > 0
    fprintf('\n =========== begin ADMM Three Block =========== \n');
end

%%%%%%% Initialize dual variebles
% rhao(1): state constraint
% rhao(2): control constraint
% rhao(3): contact constraint 
% rhao(4): velocity consensus
% rhao(5): position consensus

rhao   = Op.rhao;

admmMaxIter  = Op.admmMaxIter;

%%%%%%% Primal variables
% ddp primal
xnew = x;
unew = u;
cnew = [c;x(end,:)]; % include both centrifugal and normal forces

% ik primal
% warm start by ik
[x_ik_ws, xd_ik_ws, fk]  = kuka_second_order_IK(x_des(1:6,:), x0(1:7), x0(8:14), [0;0], zeros(7, size(x_des,2)), zeros(7, size(x_des,2)), 1);
thetalist = x_ik_ws; 
thetalistd = 0*xd_ik_ws;
x0(8:14) = thetalistd(:,1);
% projection
u_bar = zeros(size(u));
x_bar = zeros(size(x));
c_bar = zeros(size(c));

alphak_v = ones(1,admmMaxIter+1);

%%%%%%%% Dual variables
x_lambda = 0*xnew - 0*x_bar;
c_lambda = 0*cnew - 0*c_bar;
u_lambda = 0*unew - 0*u_bar;
q_lambda = 0*xnew(1:7,:) - 0*thetalist;
qd_lambda = xnew(8:14,:) - thetalistd;
% ck = (1/roll)*norm(x_lambda-x_lambda2)^2 + (1/roll)*norm(u_lambda-u_lambda2)^2 + roll*norm(x_bar-x_bar2)^2 + roll*norm(u_bar-u_bar2)^2;
    
res_u = zeros(1,admmMaxIter);
res_x = zeros(1,admmMaxIter);
res_c = zeros(1,admmMaxIter);
res_q = zeros(1,admmMaxIter);
res_qd = zeros(1,admmMaxIter);
res_ulambda = zeros(1,admmMaxIter);
res_xlambda = zeros(1,admmMaxIter);
res_clambda = zeros(1,admmMaxIter);
res_qlambda = zeros(1,admmMaxIter);
res_qdlambda = zeros(1,admmMaxIter);

costcomp = zeros(1, admmMaxIter);

plot_IK = 0;

% q_bar  = zeros(7, size(x_des,2));
% qd_bar = zeros(7, size(x_des,2));
% 
% [theta0, ~, ~]  = kuka_second_order_IK(x_des, x0(1:7), x0(8:14), [0;0], q_bar, qd_bar, 0);
% for i = 1:N
%     unew(:,i)   = -G_kuka(inertial_params, theta0(:,i))'; 
% end
    
% [Slist, M] = manipulator_POE();
p = zeros(4,4,size(xnew, 2));

%% ADMM iteration
for i = 1:admmMaxIter

    if i < 1000
    %% Original simple ADMM 
        % ====== iLQR block incorporating the soft contact model
        % robot manipulator
        % consensus: 
        fprintf('\n=========== begin iLQR %d ===========\n',i);
        [xnew, unew, ~] = iLQG_TRACK(DYNCST, x0, unew, rhao, x_bar-x_lambda, c_bar-c_lambda, u_bar-u_lambda, thetalist-q_lambda, thetalistd-qd_lambda, Op);             
        qnew            = xnew(1:7,:);
        qdnew           = xnew(8:14,:);
        %         cnew            = 0.3 * sum(xnew(15:17,:).^2, 1) ./ RC';

        %         for j = 1:size(xnew, 2)
        %             J         = Jac_kuka(xnew(1:7, j)); % jacobian at the base of the manipulator
        %             x_dot     = J * xnew(8:14, j);
        %             cnew(j)   = 0.3 * sum(x_dot(1:3).^2, 1) ./ RC(j);
        %         end

        [xd_x, xd_y, xd_z] = convert_q2x(xnew);
        [~, RC, ~] = curvature([xd_x, xd_y, xd_z]);
        RC(1) = RC(2);
        RC(end) = RC(end);
        RC(isnan(RC)) = 1000;
        RC(1:5) = 1000;
        
        for j = 1:size(xnew, 2)
            J         = Jac_kuka(xnew(1:7, j)); % jacobian at the base of the manipulator
            x_dot     = J * xnew(8:14, j);
            cnew(1,j) = 0.3 * sum(x_dot(1:3).^2, 1) ./ RC(j);
            cnew(2,j) = xnew(end,j); 
        end
        
        % ====== ik block ====== %
        fprintf('\n=========== begin IK %d ===========\n',i);
        thetalist_old = thetalist;
        thetalistd_old = thetalistd;
        
        [thetalist, thetalistd, ~]  = kuka_second_order_IK(x_des(1:6,:), x0(1:7), 0*x0(8:14), rhao(4:5), qnew+q_lambda, qdnew+qd_lambda, 1);
        

        % ====== project operator to satisfy the constraint ======= %
        x_bar_old = x_bar;
        c_bar_old = c_bar;
        u_bar_old = u_bar;
        
        [x_bar, c_bar, u_bar] = proj(xnew+x_lambda, cnew+c_lambda, unew+u_lambda, Op.lims);

        %====== dual variables update
        x_lambda = x_lambda + xnew - x_bar;
        c_lambda = c_lambda + cnew - c_bar;
        u_lambda = u_lambda + unew - u_bar;
        q_lambda = q_lambda + qnew - thetalist;
        qd_lambda = qd_lambda + qdnew - thetalistd;
        

    end
    %%
    % ====== residue ======= %
    res_u(:,i) = norm(unew - u_bar);
    res_x(:,i) = norm(xnew - x_bar);
    res_c(:,i) = norm(cnew - c_bar);
    res_q(:,i) = norm(xnew(1:7,:) - thetalist);
    res_qd(:,i) = norm(xnew(8:14,:) - thetalistd);
    
    res_ulambda(:,i) = rhao(2) * norm(u_bar - u_bar_old);
    res_xlambda(:,i) = rhao(1) * norm(x_bar - x_bar_old);
    res_clambda(:,i) = rhao(3) * norm(c_bar - c_bar_old);
    res_qlambda(:,i) = rhao(5) * norm(thetalist - thetalist_old);
    res_qdlambda(:,i) = rhao(4) * norm(thetalistd - thetalistd_old);
    
    [~,~,cost22]  = traj_sim(x0, unew, DYNCST, zeros(1,5),x_init,c_init,u_init,zeros(7,N+1),zeros(7,N+1));
    costcomp(:,i) = sum(cost22(:));
    
    % ====== varying penalty parameter ======== %
%     if i > 5
%         if res_u(:,i) > 10*res_ulambda(:,i)
%             rhao(2) = 2*rhao(2);
%             u_lambda = u_lambda/2;
%             x_lambda = x_lambda/2;
%         elseif res_ulambda(:,i) > 10*res_u(:,i)
%             rhao(1) = rhao(1)/2;
%             u_lambda = u_lambda*2;
%             x_lambda = x_lambda*2;
%         end
%     end
end

figure(15)
ppp = 1:admmMaxIter+1;
plot(ppp,alphak_v);

%% plot the residue
figure(10)
subplot(1,2,1)
l = 1:admmMaxIter;
plot(l,res_u,'DisplayName','residue u');
hold on;
plot(l,res_x,'DisplayName','residue x');
plot(l,res_c,'DisplayName','residue c');
plot(l,res_q,'DisplayName','residue q');
% plot(l,res_q_consensus,'DisplayName','residue q between ddp and ik');
% plot(l,res_qd,'DisplayName','residue qd');
% plot(l,res_ulambda,'DisplayName','residue ulambda');
% plot(l,res_xlambda,'DisplayName','residue xlambda');
% plot(l,res_clambda,'DisplayName','residue clambda');
% plot(l,res_qlambda,'DisplayName','residue qlambda');
% plot(l,res_qdlambda,'DisplayName','residue qdlambda');
title('residue of primal and dual variebles for accelerated ADMM')
xlabel('ADMM iteration')
ylabel('residue')
set(gca, 'YScale', 'log')
set(gca, 'XScale', 'linear')
legend
hold off;

subplot(1,2,2)
jj = 1:admmMaxIter;
plot(jj,costcomp);
title('cost reduction for accelerated ADMM')
xlabel('ADMM iteration')
ylabel('cost')

figure(11)
ii = 1:N;
plot(ii,unew);
hold on 
plot(ii,u_bar);

figure(12)
e1 = (unew(1,:)-u_bar(1,:))./unew(1,:);
e2 = (unew(2,:)-u_bar(2,:))./unew(2,:);
plot(ii,e1);
hold on 
plot(ii,e2);
hold off

[~,~,costnew]  = traj_sim(x0,unew,DYNCST,zeros(1,5),x_init,c_init,u_init,zeros(7,N+1),zeros(7,N+1));

%% ====== STEP 5: accept step (or not), expand or shrink trust region
% print headings
if verbosity >= 1 && last_head == print_head
    last_head = 0;
    fprintf('%-12s','iteration','cost')
    fprintf('\n');
end

% print status
if verbosity >= 1
    fprintf('%-12d%-12.6g%-12.3g\n', ...
        iter, sum(costnew(:)), 0);
    last_head = last_head+1;
end


% accept changes
u              = unew;
x              = xnew;
cost           = costnew;
% Op.plotFn(x);
   
    % update trace
%     trace(iter).lambda      = lambda;
%     trace(iter).dlambda     = dlambda;
%     trace(iter).alpha       = alpha;
%     trace(iter).improvement = dcost;
%     trace(iter).cost        = sum(cost(:));
%     trace(iter).reduc_ratio = z;
%     stop = graphics(Op.plot,x,u,cost,L,Vx,Vxx,fx,fxx,fu,fuu,trace(1:iter),0);

% save lambda/dlambda
if stop
    if verbosity > 0
        fprintf('\nEXIT: Terminated by user\n');
    end
end

if iter == Op.maxIter
    if verbosity > 0
        fprintf('\nEXIT: Maximum iterations reached.\n');
    end
end


function [xnew,unew,cnew] = traj_sim(x0, u0, DYNCST, rhao,x_bar,c_bar,u_bar, thetalist_bar, thetalistd_bar)
% Generate the initial trajectory 

    n        = size(x0,1);
    m        = size(u0,1);
    N        = size(u0,2);

    xnew        = zeros(n,N);
    xnew(:,1)   = x0(:,1);
    unew        = u0;
    cnew        = zeros(1,N+1);
    for i = 1:N
        [xnew(:,i+1), cnew(:,i)]  = DYNCST(xnew(:,i), unew(:,i),rhao,x_bar(:,i),c_bar(:,i),u_bar(:,i), thetalist_bar(:,i), thetalistd_bar(:,i),i);
        xnew(:,i+1);
    end
    
    
    [~, cnew(:,N+1)] = DYNCST(xnew(:,N+1),nan(m,1),rhao,x_bar(:,N+1),c_bar(:,N+1),u_bar(:,N+1), thetalist_bar(:,N+1), thetalistd_bar(:,N+1),i);


function [x2, c2, u2] = proj(xnew, xnew_cen, unew, lims)
    % Project operator(control-limit): simply clamp the control output
    N = size(unew, 2);
    m = size(unew, 1);
    n = size(xnew, 1);
    u2 = zeros(m,N);
    x2 = xnew;
    c2 = xnew_cen;

    for i =1:N
        if ((xnew_cen(1, i + 1)) > 0.4 * abs(xnew_cen(2, i + 1)))
            c2(1, i + 1) = 0.4 * abs(xnew_cen(2, i + 1));
        end 
        
        for j = 1:n

            if j < 8
                if xnew(j,i+1) > lims(1,2)
                    x2(j,i+1) = lims(1,2);
                elseif xnew(j,i+1) < lims(1,1)
                    x2(j,i+1) = lims(1,1);
                else
                    x2(j,i+1) = xnew(j,i+1);
                end
            else
                x2(j,i+1) = xnew(j,i+1);
            end
            
            if (j > 7 && j < 15)
                if xnew(j,i+1) > lims(2,2)
                    x2(j,i+1) = lims(2,2);
                elseif xnew(j,i+1) < lims(2,1)
                    x2(j,i+1) = lims(2,1);
                else
                    x2(j,i+1) = xnew(j,i+1);
                end
            else
                x2(j,i+1) = xnew(j,i+1);
            end
        end

        for k = 1:m
            if unew(k,i) > lims(4,2)
                u2(k,i) = lims(4,2);
            elseif unew(k,i) < lims(4,1)
                u2(k,i) = lims(4,1);
            else
                u2(k,i) = unew(k,i);
            end
        end
    end
