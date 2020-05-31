#include "kuka_arm.h"
#include <Eigen/Geometry>


KukaArm::KukaArm(double& iiwa_dt, unsigned int& iiwa_N, std::unique_ptr<KUKAModelKDL>& kukaRobot, ContactModel::SoftContactModel& contact_model, std::vector<Eigen::Matrix<double,6,1> >& iiwa_fk_ref)
{
    //#####
    globalcnt = 0;
    //#####

    if (SOFT_CONTACT)
    {
        stateNb = 20;
        q.resize((stateSize-3)/2);
        qd.resize((stateSize-3)/2);
    }
    else
    {
        stateNb = 14;
        q.resize(stateSize/2);
        qd.resize(stateSize/2);
    }
    commandNb = 7;
    dt = iiwa_dt;
    N = iiwa_N;
    fxList.resize(N);
    fuList.resize(N);

    // initialize reference cartesian trajectory
    fk_ref = iiwa_fk_ref;
    
    contact_model0 = &contact_model;

    H.setZero();
    C.setZero();
    G.setZero();
    Bu.setZero();
    Xdot_new.setZero();


    A1.setZero();
    A2.setZero();
    A3.setZero();
    A4.setZero();
    B1.setZero();
    B2.setZero();
    B3.setZero();
    B4.setZero();
    IdentityMat.setIdentity();

    Xp1.setZero();
    Xp2.setZero();
    Xp3.setZero();
    Xp4.setZero();

    Xm1.setZero();
    Xm2.setZero();
    Xm3.setZero();
    Xm4.setZero();
    
    debugging_print = 0;
    finalTimeProfile.counter0_ = 0;
    finalTimeProfile.counter1_ = 0;
    finalTimeProfile.counter2_ = 0;

    initial_phase_flag_ = 1;
    q.resize(stateSize/2);
    qd.resize(stateSize/2);

    // q_thread.resize(NUMBER_OF_THREAD);
    // qd_thread.resize(NUMBER_OF_THREAD);
    // for(unsigned int i=0;i<NUMBER_OF_THREAD;i++){
    //     q_thread[i].resize(stateSize/2);
    //     qd_thread[i].resize(stateSize/2);
    // }

    finalTimeProfile.time_period1 = 0;
    finalTimeProfile.time_period2 = 0;
    finalTimeProfile.time_period3 = 0;
    finalTimeProfile.time_period4 = 0;

    if (initial_phase_flag_ == 1)
    {
        kukaRobot_    = std::move(kukaRobot);      
        initial_phase_flag_ = 0;
    }
}

stateVec_t KukaArm::kuka_arm_dynamics(const stateVec_t& X, const commandVec_t& tau)
{
    // struct timeval tbegin_dynamics, tend_dynamics;
    // gettimeofday(&tbegin_dynamics,NULL);
    finalTimeProfile.counter0_ += 1;

    if(finalTimeProfile.counter0_ == 10)
        gettimeofday(&tbegin_period,NULL);

    if (WHOLE_BODY)
    {
        if (SOFT_CONTACT)
        {
            q = X.head((stateSize-3)/2);
            qd = X.segment((stateSize-3)/2, (stateSize-3)/2);
            force_current = X.tail(3);

            //LW-Test
            Eigen::Matrix<double,(stateSize-3)/2+2,1> q_full;
            Eigen::Matrix<double,(stateSize-3)/2+2,1> qd_full;
            Eigen::Matrix<double,(stateSize-3)/2+2,1> vd_full;
            q_full.setZero();
            qd_full.setZero();
            vd_full.setZero();
            q_full.topRows((stateSize-3)/2)=q;
            qd_full.topRows((stateSize-3)/2)=qd;

          
            Eigen::Vector3d force_dot;

            // dynamics vector
            vd.setZero();
            force_dot.setZero();

            // Xdot_new << qd, vd, force_dot;
            //LW---------------

            force_current.setZero();

            kukaRobot_->getForwardDynamics(q.data(), qd.data(), tau, qdd);
            // std::cout << qdd << std::endl;

            Xdot_new << qd, qdd, force_dot;
            // std::cout << Xdot_new << std::endl;
            // Xdot_new.setZero();

            if (finalTimeProfile.counter0_ == 10)
            {
                gettimeofday(&tend_period,NULL);
                finalTimeProfile.time_period1 += (static_cast<double>(1000.0*(tend_period.tv_sec-tbegin_period.tv_sec)+((tend_period.tv_usec-tbegin_period.tv_usec)/1000.0)))/1000.0;
            }
            

            if (globalcnt < 40) {
                globalcnt += 1;
            }

        }
       
    }

    return Xdot_new;
}


KukaArm::timeprofile KukaArm::getFinalTimeProfile()
{    
    return finalTimeProfile;
}

void KukaArm::compute_dynamics_jacobian(const stateVecTab_t& xList, const commandVecTab_t& uList)
{

    // // for a positive-definite quadratic, no control cost (indicated by the iLQG function using nans), is equivalent to u=0
    if(debugging_print) TRACE_KUKA_ARM("initialize dimensions\n");
    unsigned int Nl = xList.size();

    if(debugging_print) TRACE_KUKA_ARM("compute cost function\n");

    for (unsigned int k=0; k < Nl-1; k++) 
    {
        update_fxu(xList[k], uList[k], fx_, fu_); //assume three outputs, code needs to be optimized

        fxList[k] = fx_;
        fuList[k] = fu_; 

        /* Numdiff Eigen */
        // num_diff_.df((typename Differentiable<double, stateSize, commandSize>::InputType() << xList[k], uList[k]).finished(), j_);
        // fxList = j_;
        // fuList = j_;
    }

    
    if(debugging_print) TRACE_KUKA_ARM("finish kuka_arm_dyn_cst\n");
}


void KukaArm::update_fxu(const stateVec_t& X, const commandVec_t& U, stateMat_t& A, stateR_commandC_t& B)
{
    // 4th-order Runge-Kutta step
    if(debugging_print) TRACE_KUKA_ARM("update: 4th-order Runge-Kutta step\n");

    gettimeofday(&tbegin_period4, NULL);

    // output of kuka arm dynamics is xdot = f(x,u)
    Xdot1 = kuka_arm_dynamics(X, U);
    Xdot2 = kuka_arm_dynamics(X + 0.5*dt*Xdot1, U);
    Xdot3 = kuka_arm_dynamics(X + 0.5*dt*Xdot2, U);
    Xdot4 = kuka_arm_dynamics(X + dt*Xdot3, U);

    if(debugging_print) TRACE_KUKA_ARM("update: X_new\n");


    unsigned int n = X.size();
    unsigned int m = U.size();

    double delta = 1e-7;
    stateMat_t Dx;
    commandMat_t Du;
    Dx.setIdentity();
    Dx = delta*Dx;
    Du.setIdentity();
    Du = delta*Du;

    // State perturbation?
    for (unsigned int i = 0; i < n; i++)
    {
        Xp1 = kuka_arm_dynamics(X+Dx.col(i),U);
        Xm1 = kuka_arm_dynamics(X-Dx.col(i),U);
        A1.col(i) = (Xp1 - Xm1)/(2*delta);

        Xp2 = kuka_arm_dynamics(X+0.5*dt*Xdot1+Dx.col(i),U);
        Xm2 = kuka_arm_dynamics(X+0.5*dt*Xdot1-Dx.col(i),U);
        A2.col(i) = (Xp2 - Xm2)/(2*delta);

        Xp3 = kuka_arm_dynamics(X+0.5*dt*Xdot2+Dx.col(i),U);
        Xm3 = kuka_arm_dynamics(X+0.5*dt*Xdot2-Dx.col(i),U);
        A3.col(i) = (Xp3 - Xm3)/(2*delta);

        Xp4 = kuka_arm_dynamics(X+dt*Xdot3+Dx.col(i),U);
        Xm4 = kuka_arm_dynamics(X+dt*Xdot3-Dx.col(i),U);

        A4.col(i) = (Xp4 - Xm4)/(2*delta);
    }

    // Control perturbation?
    for (unsigned int i = 0; i < m ; i++)
    {
        Xp1 = kuka_arm_dynamics(X,U+Du.col(i));
        Xm1 = kuka_arm_dynamics(X,U-Du.col(i));
        B1.col(i) = (Xp1 - Xm1)/(2*delta);

        Xp2 = kuka_arm_dynamics(X+0.5*dt*Xdot1,U+Du.col(i));
        Xm2 = kuka_arm_dynamics(X+0.5*dt*Xdot1,U-Du.col(i));
        B2.col(i) = (Xp2 - Xm2)/(2*delta);

        Xp3 = kuka_arm_dynamics(X+0.5*dt*Xdot2,U+Du.col(i));
        Xm3 = kuka_arm_dynamics(X+0.5*dt*Xdot2,U-Du.col(i));
        B3.col(i) = (Xp3 - Xm3)/(2*delta);

        Xp4 = kuka_arm_dynamics(X+dt*Xdot3,U+Du.col(i));
        Xm4 = kuka_arm_dynamics(X+dt*Xdot3,U-Du.col(i));

        B4.col(i) = (Xp4 - Xm4)/(2*delta);
    }

    A = (IdentityMat + A4 * dt/6)*(IdentityMat + A3 * dt/3)*(IdentityMat + A2 * dt/3)*(IdentityMat + A1 * dt/6);
    B = B4 * dt/6 + (IdentityMat + A4 * dt/6) * B3 * dt/3 + (IdentityMat + A4 * dt/6)*(IdentityMat + A3 * dt/3)* B2 * dt/3 + (IdentityMat + (dt/6)*A4)*(IdentityMat + (dt/3)*A3)*(IdentityMat + (dt/3)*A2)*(dt/6)*B1;
    

    if(debugging_print) TRACE_KUKA_ARM("update: X_new\n");

    gettimeofday(&tend_period4,NULL);
    finalTimeProfile.time_period4 += (static_cast<double>(1000.0*(tend_period4.tv_sec-tbegin_period4.tv_sec)+((tend_period4.tv_usec-tbegin_period4.tv_usec)/1000.0)))/1000.0;
}


unsigned int KukaArm::getStateNb()
{
    return stateNb;
}

unsigned int KukaArm::getCommandNb()
{
    return commandNb;
}

commandVec_t& KukaArm::getLowerCommandBounds()
{
    return lowerCommandBounds;
}

commandVec_t& KukaArm::getUpperCommandBounds()
{
    return upperCommandBounds;
}

stateMatTab_t& KukaArm::getfxList()
{
    return fxList;
}

stateR_commandC_tab_t& KukaArm::getfuList()
{
    return fuList;
}

