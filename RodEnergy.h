#ifndef RODENERGY_H
#define RODENERGY_H

#include <Eigen/Core>
#include <Eigen/Sparse>
#include "RodConfig.h"

double rodEnergy(const Rod &rod, const RodState &state, Eigen::VectorXd *dE, Eigen::VectorXd *dtheta, std::vector<Eigen::Triplet<double> > *dEdw, std::vector<Eigen::Triplet<double> > *dthetadw);
double constraintEnergy(RodConfig &config, std::vector<Eigen::VectorXd> *dEs, std::vector<Eigen::VectorXd> *dthetas);

Eigen::Vector3d parallelTransport(const Eigen::Vector3d &v, const Eigen::Vector3d &e1, const Eigen::Vector3d &e2);

#endif