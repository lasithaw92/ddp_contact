function ret = fdyn_dynamics_admm_3blk(x, u, RC, K_D)
    
    % dynamics for a point mass
    global inertial_params 
    ret     = zeros(10, size(x,2));

    for i = 1:size(x,2)
        
        q         = x(1:7,i);
        q_d       = x(8:14,i);
        J         = Jac_kuka(q); % jacobian at the base of the manipulator
        
        % end effector velocity
        x_dot     = J * q_d;
        
        % friction, in the direction of sliding (tangential) and normal to
        if (norm(x_dot(1:3)) < 0.1)
            vel_dir        = x_dot(1:3);
        else
            vel_dir        = x_dot(1:3) / norm(x_dot(1:3));
        end
        
        
        % material properties of the surface
        E  = 100000; 
        R  = 0.0013/2; % in mm
        m  = 0.3;
                
        
        % the force component in the direction of spring_dir, projection to
        % the spring_dir
        spring_dir = [0 0 1]';
        
        
        % surface deformation resulting from u_z, dx in the direction of
        % spring_dir. Quasi static model. 
        d        = (9 * x(17,i) ^ 2/ (16 * E^2 * R)) ^ (1/3);
        
        % Rot = eul2rotm(x(11:13,i)', 'ZYX');
        % tool_length = Rot*[0 0 0.136]'; % end-effector frame of the tool base
        
        f_ee      = x(15:17,i);
               
        kv        = diag([4.0, 1.5, 1.0, 0.8, 0.8, 0.3, 0.05]);
        k_static  = (0.0) * diag([0.02 0.02 0.01 0.07 0.01 0.01 0.001]);
        
        Ff        =   -k_static * sign(x(8:14,i));
        Fv        =   -kv * x(8:14,i);
   
        
        grav_comp =   -G_kuka(inertial_params, x(1:7,i))'; 
        qdd       =   kukaAnalytical_qdd(inertial_params, x(1:7,i), x(8:14,i),...
            Ff+Fv+grav_comp + u(1:7,i) - J(1:3,:)' * f_ee);
        
        % acceleration in the cartesian space
        xdd_e    = J * qdd + J_dot(x) * x(8:14,i);
        
        mu       = 0.4340;
        nu       = 0.55;
        k        = (6 * E^2 * R * abs(f_ee(3))) ^ (1/3);
        
        if (k < 10)
            k = (6 * E^2 * R * abs(0.01))^(1/3);
        end
        
        % frictional force
        F_f_dot  = mu * k * x_dot(3)*vel_dir + 3 * mu * (2*nu-1) *(k * x_dot(3) * d ...
            + f_ee(3) * x_dot(3)) * vel_dir / (10 * R) + 14.02 * xdd_e;
        

        if (norm(K_D(:,i)) == 0)
            K_DIR    = K_D(:,i);
        else
            K_DIR    = K_D(:,i) ./ norm(K_D(:,i));
        end
        
        m = 1;
        F_normal_dot = 2 * m * x_dot(1:3) .* xdd_e(1:3); % / RC(i);

        F        = F_f - k * x_dot(3) * [0;0;1] - xdd_e(3) * [0;0;1] + F_normal_dot.*K_DIR;
        ret(:,i) = [qdd; 0*F];
        
    end
    
    % compute J dot
    function J_d = J_dot(x)
        e  = 0.00001;
        J1 = Jac_kuka(x(1:7));
        q_ = x(1:7) + x(8:14) * e;
        J2 = Jac_kuka(q_);
        J_d = (J2 - J1) ./ e;
    end
    


end