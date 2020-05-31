#pragma once

/// @file
///
/// kuka_plan_runner is designed to wait for LCM messages contraining
/// a robot_plan_t message, and then execute the plan on an iiwa arm
/// (also communicating via LCM using the
/// lcmt_iiwa_command/lcmt_iiwa_status messages).
///
/// When a plan is received, it will immediately begin executing that
/// plan on the arm (replacing any plan in progress).
///
/// If a stop message is received, it will immediately discard the
/// current plan and wait until a new plan is received.


#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <stdio.h>
#include <fstream>
#include <string>
#include <list>

#include <Eigen/Dense>

using Eigen::MatrixXd;
using Eigen::VectorXd;
using Eigen::VectorXi;
using Eigen::Vector2d;
using Eigen::Vector3d;

#include "config.h"
#include "spline.h"
#include "ilqrsolver.h"
#include "kuka_arm.h"
#include "SoftContactModel.h"
#include "KukaModel.h"
#include "models.h"


using namespace std;
using namespace Eigen;

/* DDP trajectory generation */

static std::list< const char*> gs_fileName;
static std::list< std::string > gs_fileName_string;


/* -------------------- Soft_contact_state = 17(14+3) ------------------------*/
class DDP 
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  DDP();

  void Run(stateVec_t xinit, stateVec_t xgoal, stateVecTab_t xtrack);

  void saveVector(const Eigen::MatrixXd & _vec, const char * _name) 
  {
      std::string _file_name = UDP_TRAJ_DIR;
      _file_name += _name;
      _file_name += ".csv";
      clean_file(_name, _file_name);

      std::ofstream save_file;
      save_file.open(_file_name, std::fstream::app);
      for (int i(0); i < _vec.rows(); ++i)
      {
          save_file<<_vec(i,0)<< "\t";
      }
      save_file<<"\n";
      save_file.flush();
      save_file.close();
  };


  void clean_file(const char * _file_name, std::string & _ret_file)
  {
      std::list<std::string>::iterator iter = std::find(gs_fileName_string.begin(), gs_fileName_string.end(), _file_name);
      if (gs_fileName_string.end() == iter)
      {
          gs_fileName_string.push_back(_file_name);
          remove(_ret_file.c_str());
      }
  };

private:
  stateVecTab_t joint_state_traj;
  commandVecTab_t torque_traj;
  stateVecTab_t joint_state_traj_interp;
  commandVecTab_t torque_traj_interp;

protected:
  optimizer::ILQRSolver::traj lastTraj;

};





