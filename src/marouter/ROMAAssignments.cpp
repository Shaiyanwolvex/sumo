/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    ROMAAssignments.cpp
/// @author  Yun-Pang Floetteroed
/// @author  Laura Bieker
/// @author  Michael Behrisch
/// @date    Feb 2013
/// @version $Id$
///
// Assignment methods
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#include <config.h>

#include <vector>
#include <algorithm>
#include <router/ROEdge.h>
#include <utils/vehicle/RouteCostCalculator.h>
#include <router/RONet.h>
#include <router/RORoute.h>
#include <utils/distribution/Distribution_Points.h>
#include <od/ODMatrix.h>
#include <utils/common/SUMOTime.h>
#include <utils/vehicle/SUMOAbstractRouter.h>
#include "ROMAEdge.h"
#include "ROMAAssignments.h"


// ===========================================================================
// static member variables
// ===========================================================================
std::map<const ROEdge* const, double> ROMAAssignments::myPenalties;


// ===========================================================================
// method definitions
// ===========================================================================

ROMAAssignments::ROMAAssignments(const SUMOTime begin, const SUMOTime end, const bool additiveTraffic,
                                 const double adaptionFactor, RONet& net, ODMatrix& matrix,
                                 SUMOAbstractRouter<ROEdge, ROVehicle>& router)
    : myBegin(begin), myEnd(end), myAdditiveTraffic(additiveTraffic), myAdaptionFactor(adaptionFactor), myNet(net), myMatrix(matrix), myRouter(router) {
    myDefaultVehicle = new ROVehicle(SUMOVehicleParameter(), nullptr, net.getVehicleTypeSecure(DEFAULT_VTYPE_ID), &net);
}


ROMAAssignments::~ROMAAssignments() {
    delete myDefaultVehicle;
}

// based on the definitions in PTV-Validate and in the VISUM-Cologne network
double
ROMAAssignments::getCapacity(const ROEdge* edge) {
    if (edge->isTazConnector()) {
        return 0;
    }
    const int roadClass = -edge->getPriority();
    // TODO: differ road class 1 from the unknown road class 1!!!
    if (edge->getNumLanes() == 0) {
        // TAZ have no cost
        return 0;
    } else if (roadClass == 0 || roadClass == 1)  {
        return edge->getNumLanes() * 2000.; //CR13 in table.py
    } else if (roadClass == 2 && edge->getSpeedLimit() <= 11.) {
        return edge->getNumLanes() * 1333.33; //CR5 in table.py
    } else if (roadClass == 2 && edge->getSpeedLimit() > 11. && edge->getSpeedLimit() <= 16.) {
        return edge->getNumLanes() * 1500.; //CR3 in table.py
    } else if (roadClass == 2 && edge->getSpeedLimit() > 16.) {
        return edge->getNumLanes() * 2000.; //CR13 in table.py
    } else if (roadClass == 3 && edge->getSpeedLimit() <= 11.) {
        return edge->getNumLanes() * 800.; //CR5 in table.py
    } else if (roadClass == 3 && edge->getSpeedLimit() > 11. && edge->getSpeedLimit() <= 13.) {
        return edge->getNumLanes() * 875.; //CR5 in table.py
    } else if (roadClass == 3 && edge->getSpeedLimit() > 13. && edge->getSpeedLimit() <= 16.) {
        return edge->getNumLanes() * 1500.; //CR4 in table.py
    } else if (roadClass == 3 && edge->getSpeedLimit() > 16.) {
        return edge->getNumLanes() * 1800.; //CR13 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() <= 5.) {
        return edge->getNumLanes() * 200.; //CR7 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 5. && edge->getSpeedLimit() <= 7.) {
        return edge->getNumLanes() * 412.5; //CR7 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 7. && edge->getSpeedLimit() <= 9.) {
        return edge->getNumLanes() * 600.; //CR6 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 9. && edge->getSpeedLimit() <= 11.) {
        return edge->getNumLanes() * 800.; //CR5 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 11. && edge->getSpeedLimit() <= 13.) {
        return edge->getNumLanes() * 1125.; //CR5 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 13. && edge->getSpeedLimit() <= 16.) {
        return edge->getNumLanes() * 1583.; //CR4 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 16. && edge->getSpeedLimit() <= 18.) {
        return edge->getNumLanes() * 1100.; //CR3 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 18. && edge->getSpeedLimit() <= 22.) {
        return edge->getNumLanes() * 1200.; //CR3 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 22. && edge->getSpeedLimit() <= 26.) {
        return edge->getNumLanes() * 1300.; //CR3 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 26.) {
        return edge->getNumLanes() * 1400.; //CR3 in table.py
    }
    return edge->getNumLanes() * 800.; //CR5 in table.py
}


// based on the definitions in PTV-Validate and in the VISUM-Cologne network
double
ROMAAssignments::capacityConstraintFunction(const ROEdge* edge, const double flow) const {
    if (edge->isTazConnector()) {
        return 0;
    }
    const int roadClass = -edge->getPriority();
    const double capacity = getCapacity(edge);
    // TODO: differ road class 1 from the unknown road class 1!!!
    if (edge->getNumLanes() == 0) {
        // TAZ have no cost
        return 0;
    } else if (roadClass == 0 || roadClass == 1)  {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 1.3)) * 2.); //CR13 in table.py
    } else if (roadClass == 2 && edge->getSpeedLimit() <= 11.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.9)) * 3.); //CR5 in table.py
    } else if (roadClass == 2 && edge->getSpeedLimit() > 11. && edge->getSpeedLimit() <= 16.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 1.)) * 2.); //CR3 in table.py
    } else if (roadClass == 2 && edge->getSpeedLimit() > 16.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 1.3)) * 2.); //CR13 in table.py
    } else if (roadClass == 3 && edge->getSpeedLimit() <= 11.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.9)) * 3.); //CR5 in table.py
    } else if (roadClass == 3 && edge->getSpeedLimit() > 11. && edge->getSpeedLimit() <= 13.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.9)) * 3.); //CR5 in table.py
    } else if (roadClass == 3 && edge->getSpeedLimit() > 13. && edge->getSpeedLimit() <= 16.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.7 * (flow / (capacity * 1.)) * 2.); //CR4 in table.py
    } else if (roadClass == 3 && edge->getSpeedLimit() > 16.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 1.3)) * 2.); //CR13 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() <= 5.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.5)) * 3.); //CR7 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 5. && edge->getSpeedLimit() <= 7.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.5)) * 3.); //CR7 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 7. && edge->getSpeedLimit() <= 9.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.8)) * 3.); //CR6 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 9. && edge->getSpeedLimit() <= 11.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.9)) * 3.); //CR5 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 11. && edge->getSpeedLimit() <= 13.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.9)) * 3.); //CR5 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 13. && edge->getSpeedLimit() <= 16.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.7 * (flow / (capacity * 1.)) * 2.); //CR4 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 16. && edge->getSpeedLimit() <= 18.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 1.)) * 2.); //CR3 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 18. && edge->getSpeedLimit() <= 22.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 1.)) * 2.); //CR3 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 22. && edge->getSpeedLimit() <= 26.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 1.)) * 2.); //CR3 in table.py
    } else if ((roadClass >= 4 || roadClass == -1) && edge->getSpeedLimit() > 26.) {
        return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 1.)) * 2.); //CR3 in table.py
    }
    return edge->getLength() / edge->getSpeedLimit() * (1. + 1.*(flow / (capacity * 0.9)) * 3.); //CR5 in table.py
}


bool
ROMAAssignments::addRoute(const ConstROEdgeVector& edges, std::vector<RORoute*>& paths, std::string routeId, double prob) {
    std::vector<RORoute*>::iterator p;
    for (p = paths.begin(); p != paths.end(); p++) {
        if (edges == (*p)->getEdgeVector()) {
            break;
        }
    }
    if (p == paths.end()) {
        paths.push_back(new RORoute(routeId, 0., prob, edges, nullptr, std::vector<SUMOVehicleParameter::Stop>()));
        return true;
    }
    (*p)->addProbability(prob);
    std::iter_swap(paths.end() - 1, p);
    return false;
}


const ConstROEdgeVector
ROMAAssignments::computePath(const ODCell* cell, const SUMOTime time, SUMOAbstractRouter<ROEdge, ROVehicle>* router) {
    const ROEdge* const from = myNet.getEdge(cell->origin + (cell->originIsEdge ? "" : "-source"));
    if (from == nullptr) {
        throw ProcessError("Unknown origin '" + cell->origin + "'.");
    }
    const ROEdge* const to = myNet.getEdge(cell->destination + (cell->destinationIsEdge ? "" : "-sink"));
    if (to == nullptr) {
        throw ProcessError("Unknown destination '" + cell->destination + "'.");
    }
    ConstROEdgeVector edges;
    if (router == nullptr) {
        router = &myRouter;
    }
    router->compute(from, to, myDefaultVehicle, time, edges);
    return edges;
}


void
ROMAAssignments::getKPaths(const int kPaths, const double penalty) {
    for (ODCell* const c : myMatrix.getCells()) {
        myPenalties.clear();
        for (int k = 0; k < kPaths; k++) {
            ConstROEdgeVector edges = computePath(c);
            for (ConstROEdgeVector::iterator e = edges.begin(); e != edges.end(); e++) {
                myPenalties[*e] = penalty;
            }
            addRoute(edges, c->pathsVector, c->origin + c->destination + toString(c->pathsVector.size()), 0);
        }
    }
    myPenalties.clear();
}


void
ROMAAssignments::resetFlows() {
    const double begin = STEPS2TIME(MIN2(myBegin, myMatrix.getCells().front()->begin));
    for (std::map<std::string, ROEdge*>::const_iterator i = myNet.getEdgeMap().begin(); i != myNet.getEdgeMap().end(); ++i) {
        ROMAEdge* edge = static_cast<ROMAEdge*>(i->second);
        edge->setFlow(begin, STEPS2TIME(myEnd), 0.);
        edge->setHelpFlow(begin, STEPS2TIME(myEnd), 0.);
    }
}


void
ROMAAssignments::incremental(const int numIter, const bool verbose) {
    SUMOTime lastBegin = -1;
    std::vector<int> intervals;
    int count = 0;
    for (const ODCell* const c : myMatrix.getCells()) {
        if (c->begin != lastBegin) {
            intervals.push_back(count);
            lastBegin = c->begin;
        }
        count++;
    }
    lastBegin = -1;
    for (std::vector<int>::const_iterator offset = intervals.begin(); offset != intervals.end(); offset++) {
        std::vector<ODCell*>::const_iterator cellsEnd = myMatrix.getCells().end();
        if (offset != intervals.end() - 1) {
            cellsEnd = myMatrix.getCells().begin() + (*(offset + 1));
        }
        const SUMOTime intervalStart = (*(myMatrix.getCells().begin() + (*offset)))->begin;
        if (verbose) {
            WRITE_MESSAGE(" starting interval " + time2string(intervalStart));
        }
        std::map<const ROMAEdge*, double> loadedTravelTimes;
        for (std::map<std::string, ROEdge*>::const_iterator i = myNet.getEdgeMap().begin(); i != myNet.getEdgeMap().end(); ++i) {
            ROMAEdge* edge = static_cast<ROMAEdge*>(i->second);
            if (edge->hasLoadedTravelTime(STEPS2TIME(intervalStart))) {
                loadedTravelTimes[edge] = edge->getTravelTime(myDefaultVehicle, STEPS2TIME(intervalStart));
            }
        }
        for (int t = 0; t < numIter; t++) {
            if (verbose) {
                WRITE_MESSAGE("  starting iteration " + toString(t));
            }
            std::string lastOrigin = "";
            int workerIndex = 0;
            for (std::vector<ODCell*>::const_iterator i = myMatrix.getCells().begin() + (*offset); i != cellsEnd; i++) {
                ODCell* const c = *i;
                const double linkFlow = c->vehicleNumber / numIter;
                const SUMOTime begin = myAdditiveTraffic ? myBegin : c->begin;
#ifdef HAVE_FOX
                if (myNet.getThreadPool().size() > 0) {
                    if (lastOrigin != c->origin) {
                        workerIndex++;
                        if (workerIndex == myNet.getThreadPool().size()) {
                            workerIndex = 0;
                        }
                        myNet.getThreadPool().add(new RONet::BulkmodeTask(false), workerIndex);
                        lastOrigin = c->origin;
                        myNet.getThreadPool().add(new RoutingTask(*this, c, begin, linkFlow), workerIndex);
                        myNet.getThreadPool().add(new RONet::BulkmodeTask(true), workerIndex);
                    } else {
                        myNet.getThreadPool().add(new RoutingTask(*this, c, begin, linkFlow), workerIndex);
                    }
                    continue;
                }
#endif
                if (lastOrigin != c->origin) {
                    myRouter.setBulkMode(false);
                    lastOrigin = c->origin;
                }
                const ConstROEdgeVector& edges = computePath(c, begin);
                myRouter.setBulkMode(true);
                addRoute(edges, c->pathsVector, c->origin + c->destination + toString(c->pathsVector.size()), linkFlow);
            }
#ifdef HAVE_FOX
            if (myNet.getThreadPool().size() > 0) {
                myNet.getThreadPool().waitAll();
            }
#endif
            for (std::vector<ODCell*>::const_iterator i = myMatrix.getCells().begin() + (*offset); i != cellsEnd; i++) {
                ODCell* const c = *i;
                const double linkFlow = c->vehicleNumber / numIter;
                const SUMOTime begin = myAdditiveTraffic ? myBegin : c->begin;
                const SUMOTime end = myAdditiveTraffic ? myEnd : c->end;
                const double intervalLengthInHours = STEPS2TIME(end - begin) / 3600.;
                const ConstROEdgeVector& edges = c->pathsVector.back()->getEdgeVector();
                for (ConstROEdgeVector::const_iterator e = edges.begin(); e != edges.end(); e++) {
                    ROMAEdge* edge = static_cast<ROMAEdge*>(myNet.getEdge((*e)->getID()));
                    const double newFlow = edge->getFlow(STEPS2TIME(begin)) + linkFlow;
                    edge->setFlow(STEPS2TIME(begin), STEPS2TIME(end), newFlow);
                    double travelTime = capacityConstraintFunction(edge, newFlow / intervalLengthInHours);
                    if (lastBegin >= 0 && myAdaptionFactor > 0.) {
                        if (loadedTravelTimes.count(edge) != 0) {
                            travelTime = loadedTravelTimes[edge] * myAdaptionFactor + (1. - myAdaptionFactor) * travelTime;
                        } else {
                            travelTime = edge->getTravelTime(myDefaultVehicle, STEPS2TIME(lastBegin)) * myAdaptionFactor + (1. - myAdaptionFactor) * travelTime;
                        }
                    }
                    edge->addTravelTime(travelTime, STEPS2TIME(begin), STEPS2TIME(end));
                }
            }
        }
        lastBegin = intervalStart;
    }
}


void
ROMAAssignments::sue(const int maxOuterIteration, const int maxInnerIteration, const int kPaths, const double penalty, const double tolerance, const std::string /* routeChoiceMethod */) {
    getKPaths(kPaths, penalty);
    std::map<const double, double> intervals;
    if (myAdditiveTraffic) {
        intervals[STEPS2TIME(myBegin)] = STEPS2TIME(myEnd);
    } else {
        for (const ODCell* const c : myMatrix.getCells()) {
            intervals[STEPS2TIME(c->begin)] = STEPS2TIME(c->end);
        }
    }
    for (int outer = 0; outer < maxOuterIteration; outer++) {
        for (int inner = 0; inner < maxInnerIteration; inner++) {
            for (const ODCell* const c : myMatrix.getCells()) {
                const SUMOTime begin = myAdditiveTraffic ? myBegin : c->begin;
                const SUMOTime end = myAdditiveTraffic ? myEnd : c->end;
                // update path cost
                for (std::vector<RORoute*>::const_iterator j = c->pathsVector.begin(); j != c->pathsVector.end(); ++j) {
                    RORoute* r = *j;
                    r->setCosts(myRouter.recomputeCosts(r->getEdgeVector(), myDefaultVehicle, 0));
//                    std::cout << std::setprecision(20) << r->getID() << ":" << r->getCosts() << std::endl;
                }
                // calculate route utilities and probabilities
                RouteCostCalculator<RORoute, ROEdge, ROVehicle>::getCalculator().calculateProbabilities(c->pathsVector, myDefaultVehicle, 0);
                // calculate route flows
                for (std::vector<RORoute*>::const_iterator j = c->pathsVector.begin(); j != c->pathsVector.end(); ++j) {
                    RORoute* r = *j;
                    const double pathFlow = r->getProbability() * c->vehicleNumber;
                    // assign edge flow deltas
                    for (ConstROEdgeVector::const_iterator e = r->getEdgeVector().begin(); e != r->getEdgeVector().end(); e++) {
                        ROMAEdge* edge = static_cast<ROMAEdge*>(myNet.getEdge((*e)->getID()));
                        edge->setHelpFlow(STEPS2TIME(begin), STEPS2TIME(end), edge->getHelpFlow(STEPS2TIME(begin)) + pathFlow);
                    }
                }
            }
            // calculate new edge flows and check for stability
            int unstableEdges = 0;
            for (std::map<const double, double>::const_iterator i = intervals.begin(); i != intervals.end(); ++i) {
                const double intervalLengthInHours = STEPS2TIME(i->second - i->first) / 3600.;
                for (std::map<std::string, ROEdge*>::const_iterator e = myNet.getEdgeMap().begin(); e != myNet.getEdgeMap().end(); ++e) {
                    ROMAEdge* edge = static_cast<ROMAEdge*>(e->second);
                    const double oldFlow = edge->getFlow(i->first);
                    double newFlow = oldFlow;
                    if (inner == 0 && outer == 0) {
                        newFlow += edge->getHelpFlow(i->first);
                    } else {
                        newFlow += (edge->getHelpFlow(i->first) - oldFlow) / (inner + 1);
                    }
                    //                if not lohse:
                    if (newFlow > 0.) {
                        if (fabs(newFlow - oldFlow) / newFlow > tolerance) {
                            unstableEdges++;
                        }
                    } else if (newFlow == 0.) {
                        if (oldFlow != 0. && (fabs(newFlow - oldFlow) / oldFlow > tolerance)) {
                            unstableEdges++;
                        }
                    } else { // newFlow < 0.
                        unstableEdges++;
                        newFlow = 0.;
                    }
                    edge->setFlow(i->first, i->second, newFlow);
                    const double travelTime = capacityConstraintFunction(edge, newFlow / intervalLengthInHours);
                    edge->addTravelTime(travelTime, i->first, i->second);
                    edge->setHelpFlow(i->first, i->second, 0.);
                }
            }
            // if stable break
            if (unstableEdges == 0) {
                break;
            }
            // additional stability check from python script: if notstable < math.ceil(net.geteffEdgeCounts()*0.005) or notstable < 3: stable = True
        }
        // check for a new route, if none available, break
        // several modifications about when a route is new and when to break are in the original script
        bool newRoute = false;
        for (ODCell* const c : myMatrix.getCells()) {
            const ConstROEdgeVector& edges = computePath(c);
            newRoute |= addRoute(edges, c->pathsVector, c->origin + c->destination + toString(c->pathsVector.size()), 0);
        }
        if (!newRoute) {
            break;
        }
    }
    // final round of assignment
    for (const ODCell* const c : myMatrix.getCells()) {
        // update path cost
        for (std::vector<RORoute*>::const_iterator j = c->pathsVector.begin(); j != c->pathsVector.end(); ++j) {
            RORoute* r = *j;
            r->setCosts(myRouter.recomputeCosts(r->getEdgeVector(), myDefaultVehicle, 0));
        }
        // calculate route utilities and probabilities
        RouteCostCalculator<RORoute, ROEdge, ROVehicle>::getCalculator().calculateProbabilities(c->pathsVector, myDefaultVehicle, 0);
        // calculate route flows
        for (std::vector<RORoute*>::const_iterator j = c->pathsVector.begin(); j != c->pathsVector.end(); ++j) {
            RORoute* r = *j;
            r->setProbability(r->getProbability() * c->vehicleNumber);
        }
    }
}


double
ROMAAssignments::getPenalizedEffort(const ROEdge* const e, const ROVehicle* const v, double t) {
    const std::map<const ROEdge* const, double>::const_iterator i = myPenalties.find(e);
    return i == myPenalties.end() ? e->getEffort(v, t) : e->getEffort(v, t) + i->second;
}


double
ROMAAssignments::getPenalizedTT(const ROEdge* const e, const ROVehicle* const v, double t) {
    const std::map<const ROEdge* const, double>::const_iterator i = myPenalties.find(e);
    return i == myPenalties.end() ? e->getTravelTime(v, t) : e->getTravelTime(v, t) + i->second;
}


double
ROMAAssignments::getTravelTime(const ROEdge* const e, const ROVehicle* const v, double t) {
    return e->getTravelTime(v, t);
}


#ifdef HAVE_FOX
// ---------------------------------------------------------------------------
// ROMAAssignments::RoutingTask-methods
// ---------------------------------------------------------------------------
void
ROMAAssignments::RoutingTask::run(FXWorkerThread* context) {
    const ConstROEdgeVector& edges = myAssign.computePath(myCell, myBegin, &static_cast<RONet::WorkerThread*>(context)->getVehicleRouter());
    myAssign.addRoute(edges, myCell->pathsVector, myCell->origin + myCell->destination + toString(myCell->pathsVector.size()), myLinkFlow);
}
#endif
