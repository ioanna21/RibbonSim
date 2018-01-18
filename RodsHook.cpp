#include "RodsHook.h"
#include "RodParser.h"

void RodsHook::initGUI(igl::viewer::Viewer &viewer)
{
    dt = 1e-7;
    damp = 100;
    savePrefix = "rod_";
    loadName = "../configs/torus.rod";

    viewer.ngui->addVariable("Config File", loadName);

    viewer.ngui->addGroup("Sim Options");
    viewer.ngui->addVariable("Time Step", dt);
    viewer.ngui->addVariable("Damping Factor", damp);
    viewer.ngui->addButton("Save Geometry", std::bind(&RodsHook::saveRods, this));
    viewer.ngui->addVariable("Save Prefix", savePrefix);
    
    viewer.ngui->addGroup("Sim Status");
    viewer.ngui->addVariable("Iteration", iter, false);
    viewer.ngui->addVariable("Force Residual", forceResidual, false);
}

void RodsHook::initSimulation()
{
    iter = 0;
    forceResidual = 0;

    if (config)
        delete config;

    config = readRod(loadName.c_str());
    if (!config)
        exit(-1);
   
    createVisualizationMesh();
    dirty = true;
}


void RodsHook::showForces(int rod, const Eigen::VectorXd &dE)
{
    int nverts = config->rods[rod]->numVertices();
    forcePoints.resize(2*nverts, 3);
    forceEdges.resize(nverts, 2);
    forceColors.resize(nverts, 3);
    for (int i = 0; i < nverts; i++)
    {
        forcePoints.row(2 * i) = config->rods[rod]->curState.centerline.row(i);
        forcePoints.row(2 * i+1) = config->rods[rod]->curState.centerline.row(i) - dE.segment<3>(3*i).transpose();
        forceEdges(i, 0) = 2 * i;
        forceEdges(i, 1) = 2 * i + 1;
        forceColors.row(i) = Eigen::Vector3d(1, 0, 0);
    }
}

void RodsHook::createVisualizationMesh()
{
    config->createVisualizationMesh(Q, F);
}

double lineSearch(RodConfig &config, const Eigen::VectorXd &update)
{
    double t = 1.0;
    double c1 = 0.1;
    double c2 = 0.9;
    double alpha = 0;
    double infinity = 1e6;
    double beta = infinity;

    Eigen::VectorXd r;
    Eigen::SparseMatrix<double> J;
    rAndJ(config, r, &J);
    
    Eigen::VectorXd dE;
    Eigen::VectorXd newdE;
    std::vector<RodState> start;
    for (int i = 0; i < config.numRods(); i++)
        start.push_back(config.rods[i]->curState);
    
    double orig = 0.5 * r.squaredNorm();
    dE = J.transpose() * r;
    double deriv = -dE.dot(update);
    assert(deriv < 0);

    std::cout << "Starting line search, original energy " << orig << ", descent magnitude " << deriv << std::endl;

    while (true)
    {
        int nrods = config.numRods();
        int dofoffset = 0;
        for (int rod = 0; rod < nrods; rod++)
        {
            int nverts = config.rods[rod]->numVertices();
            int nsegs = config.rods[rod]->numSegments();
            for (int i = 0; i < nsegs; i++)
            {
                Eigen::Vector3d oldv1 =start[rod].centerline.row(i);
                Eigen::Vector3d oldv2 = start[rod].centerline.row((i + 1) % nverts);
                Eigen::Vector3d v1 = oldv1 - t * update.segment<3>(dofoffset + 3 * i);
                Eigen::Vector3d v2 = oldv2 - t * update.segment<3>(dofoffset + 3 * ((i + 1) % nverts));

                config.rods[rod]->curState.directors.row(i) = parallelTransport(start[rod].directors.row(i), oldv2 - oldv1, v2 - v1);
            }
            for (int i = 0; i < nverts; i++)
                config.rods[rod]->curState.centerline.row(i) = start[rod].centerline.row(i) - t * update.segment<3>(dofoffset + 3 * i).transpose();
            for (int i = 0; i < nsegs; i++)
                config.rods[rod]->curState.thetas[i] = start[rod].thetas[i] - t * update[dofoffset + 3 * nverts + i];

            dofoffset += 3 * nverts + 2 * nsegs;
        }
        
        rAndJ(config, r, &J);

        double newenergy = 0.5 * r.squaredNorm();
        newdE = J.transpose() * r;

        std::cout << "Trying t = " << t << ", energy now " << newenergy << std::endl;

        if (std::isnan(newenergy) || newenergy > orig + t*deriv*c1)
        {
            beta = t;
            t = 0.5*(alpha + beta);
        }
        else if (-newdE.dot(update) < c2*deriv)
        {
            alpha = t;
            if (beta == infinity)
            {
                t = 2 * alpha;
            }
            else
            {
                t = 0.5*(alpha + beta);
            }

            if (beta - alpha < 1e-8)
            {
                return t;
            }
        }
        else
        {
            return t;
        }
    }
}

bool RodsHook::simulateOneStep()
{
    //config->rods[0]->params.kstretching = 0;
    //config->rods[0]->params.ktwist = 0;
    //config->rods[0]->params.kbending = 0;
    Eigen::VectorXd r;
    Eigen::SparseMatrix<double> Jr;
    rAndJ(*config, r, &Jr);

    std::cout << "Orig energy: " << r.squaredNorm() << std::endl;
    Eigen::SparseMatrix<double> mat = Jr.transpose() * Jr;
    Eigen::SparseMatrix<double> I(mat.rows(), mat.cols());
    I.setIdentity();    
    double reg = 1e-6;
    mat += reg*I;

    Eigen::VectorXd rhs = Jr.transpose() * r;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double> > solver(mat);
    Eigen::VectorXd delta = solver.solve(rhs);
    if (solver.info() != Eigen::Success)
        exit(-1);
    std::cout << "Solver residual: " << (mat*delta - rhs).norm() << std::endl;
    lineSearch(*config, delta);

    createVisualizationMesh();    

    double newresid = 0;

    return false;
}

void RodsHook::saveRods()
{
    config->saveRodGeometry(savePrefix);
}