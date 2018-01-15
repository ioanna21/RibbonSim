#include "RodConfig.h"
#include <iostream>
#include <sstream>
#include <igl/writeOBJ.h>
#include <igl/read_triangle_mesh.h>


#include <Eigen/Geometry>


RodConfig::~RodConfig()
{
    int nrods = numRods();
    for (int i = 0; i < nrods; i++)
        delete rods[i];
}

void RodConfig::addRod(Rod *rod)
{
    rods.push_back(rod);
}

void RodConfig::addConstraint(Constraint c)
{
    assert(c.rod1 >= 0 && c.rod1 < numRods());
    assert(c.rod2 >= 0 && c.rod2 < numRods());
    assert(c.seg1 >= 0 && c.seg1 < rods[c.rod1]->numSegments());
    assert(c.seg2 >= 0 && c.seg2 < rods[c.rod2]->numSegments());
    assert(c.bary1 >= 0.0 && c.bary1 <= 1.0);
    assert(c.bary2 >= 0.0 && c.bary2 <= 1.0);
    constraints.push_back(c);
}

void RodConfig::reset()
{
    int nrods = (int)rods.size();
    for (int i = 0; i < nrods; i++)
    {
        rods[i]->curState = rods[i]->startState;
    }
}

bool RodConfig::loadTargetMesh(const std::string &objname)
{
    Eigen::MatrixXd Vtmp;
    if (!igl::read_triangle_mesh(objname, Vtmp, F_mesh))
    {
        std::cerr << "Couldn't load mesh " << objname << std::endl;
        return false;
    }
    if (Vtmp.cols() < 3)
    {
        std::cerr << "Mesh must 3D" << std::endl;
        return false;
    }
    V_mesh.resize(Vtmp.rows(), 3);
    //wtf
    for (int i = 0; i < 3; i++)
    {
        V_mesh.col(i) = Vtmp.col(i);
    }

    mesh_tree.init(V_mesh, F_mesh);

    for (int i = 0; i < numRods(); i++)
    {
        rods[i]->tree = &mesh_tree; 
        rods[i]->updateProjectionVars( V_mesh, F_mesh );
        rods[i]->startState.closestFaceNormals = rods[i]->curState.closestFaceNormals;
        rods[i]->startState.closestFaceCentroids = rods[i]->curState.closestFaceCentroids;
    }
    
    return true;
}

void RodConfig::createVisualizationMesh(Eigen::MatrixXd &Q, Eigen::MatrixXi &F)
{
    int nrods = (int)rods.size();
    int totalsegs = 0;
    for (int i = 0; i < nrods; i++)
    {
        totalsegs += rods[i]->numSegments();
    }
    Q.resize(8 * totalsegs, 3);
    F.resize(8 * totalsegs, 3);

    int offset = 0;

    for (int rod = 0; rod < nrods; rod++)
    {
        int nverts = rods[rod]->numVertices();
        int nsegs = rods[rod]->numSegments();
        Eigen::MatrixXd N(nsegs, 3);
        Eigen::MatrixXd B(nsegs, 3);
        for (int i = 0; i < nsegs; i++)
        {
            Eigen::Vector3d v0 = rods[rod]->curState.centerline.row(i);
            Eigen::Vector3d v1 = rods[rod]->curState.centerline.row((i + 1) % nverts);
            Eigen::Vector3d e = v1 - v0;
            e /= e.norm();
            Eigen::Vector3d d1 = rods[rod]->curState.directors.row(i);
            Eigen::Vector3d d2 = e.cross(d1);
            double theta = rods[rod]->curState.thetas[i];
            N.row(i) = d1*cos(theta) + d2*sin(theta);
            B.row(i) = -d1*sin(theta) + d2*cos(theta);
        }

        for (int i = 0; i < nsegs; i++)
        {
            Eigen::Vector3d v0 = rods[rod]->curState.centerline.row(i);
            Eigen::Vector3d v1 = rods[rod]->curState.centerline.row((i + 1) % nverts);
            Eigen::Vector3d T = v1 - v0;
            T /= T.norm();
            Q.row(offset + 8 * i + 0) = (v0.transpose() + rods[rod]->params.thickness / 2.0 * N.row(i) - rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(offset + 8 * i + 1) = (v0.transpose() + rods[rod]->params.thickness / 2.0 * N.row(i) + rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(offset + 8 * i + 2) = (v0.transpose() - rods[rod]->params.thickness / 2.0 * N.row(i) + rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(offset + 8 * i + 3) = (v0.transpose() - rods[rod]->params.thickness / 2.0 * N.row(i) - rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(offset + 8 * i + 4) = (v1.transpose() + rods[rod]->params.thickness / 2.0 * N.row(i) - rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(offset + 8 * i + 5) = (v1.transpose() + rods[rod]->params.thickness / 2.0 * N.row(i) + rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(offset + 8 * i + 6) = (v1.transpose() - rods[rod]->params.thickness / 2.0 * N.row(i) + rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(offset + 8 * i + 7) = (v1.transpose() - rods[rod]->params.thickness / 2.0 * N.row(i) - rods[rod]->widths[i] / 2.0 * B.row(i));
            for (int j = 0; j < 4; j++)
            {
                F(offset + 8 * i + 2 * j, 0) = offset + 8 * i + j;
                F(offset + 8 * i + 2 * j, 2) = offset + 8 * i + 4 + j;
                F(offset + 8 * i + 2 * j, 1) = offset + 8 * i + 4 + ((j + 1) % 4);
                F(offset + 8 * i + 2 * j + 1, 0) = offset + 8 * i + (j + 1) % 4;
                F(offset + 8 * i + 2 * j + 1, 2) = offset + 8 * i + j;
                F(offset + 8 * i + 2 * j + 1, 1) = offset + 8 * i + 4 + ((j + 1) % 4);
            }
        }
        offset += 8 * rods[rod]->numSegments();
    }
}

void RodConfig::saveRodGeometry(const std::string &prefix)
{
    int nrods = (int)rods.size();
    
    for (int rod = 0; rod < nrods; rod++)
    {
        int nverts = rods[rod]->numVertices();
        int nsegs = rods[rod]->numSegments();
        Eigen::MatrixXd N(nsegs, 3);
        Eigen::MatrixXd B(nsegs, 3);
        for (int i = 0; i < nsegs; i++)
        {
            Eigen::Vector3d v0 = rods[rod]->curState.centerline.row(i);
            Eigen::Vector3d v1 = rods[rod]->curState.centerline.row((i + 1) % nverts);
            Eigen::Vector3d e = v1 - v0;
            e /= e.norm();
            Eigen::Vector3d d1 = rods[rod]->curState.directors.row(i);
            Eigen::Vector3d d2 = e.cross(d1);
            double theta = rods[rod]->curState.thetas[i];
            N.row(i) = d1*cos(theta) + d2*sin(theta);
            B.row(i) = -d1*sin(theta) + d2*cos(theta);
        }

        Eigen::MatrixXd Q(2 * nverts, 3);
        Eigen::MatrixXi F(2 * nsegs, 3);

        for (int i = 0; i < nsegs; i++)
        {
            Eigen::Vector3d v0 = rods[rod]->curState.centerline.row(i);
            Eigen::Vector3d v1 = rods[rod]->curState.centerline.row((i + 1) % nverts);
            Eigen::Vector3d T = v1 - v0;
            T /= T.norm();

            double weight;
            if ((i == 0 || i == nsegs - 1) && !rods[rod]->isClosed())
                weight = 1.0;
            else weight = 0.5;

            Q.row(2 * i + 0) = (v0.transpose() - rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(2 * i + 1) = (v0.transpose() + rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(2 * i + 2) = (v1.transpose() - rods[rod]->widths[i] / 2.0 * B.row(i));
            Q.row(2 * i + 3) = (v1.transpose() + rods[rod]->widths[i] / 2.0 * B.row(i));
            
            F(2 * i + 0, 0) = 2 * i + 0;
            F(2 * i + 0, 1) = 2 * i + 1;
            F(2 * i + 0, 2) = 2 * i + 2;
            F(2 * i + 1, 0) = 2 * i + 2;
            F(2 * i + 1, 1) = 2 * i + 1;
            F(2 * i + 1, 2) = 2 * i + 3;
        }

        std::stringstream ss;
        ss << prefix << rod << ".obj";
        igl::writeOBJ(ss.str(), Q, F);
    }
}