/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    GNECrossing.cpp
/// @author  Jakob Erdmann
/// @date    June 2011
/// @version $Id$
///
// A class for visualizing Inner Lanes (used when editing traffic lights)
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#include <config.h>

#include <string>
#include <iostream>
#include <utility>
#include <time.h>
#include <utils/foxtools/MFXUtils.h>
#include <utils/geom/PositionVector.h>
#include <utils/geom/GeomConvHelper.h>
#include <utils/gui/windows/GUISUMOAbstractView.h>
#include <utils/common/ToString.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/globjects/GUIGLObjectPopupMenu.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/globjects/GLIncludes.h>
#include <netbuild/NBTrafficLightLogic.h>
#include <netedit/GNEViewParent.h>
#include <netedit/GNEViewNet.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNENet.h>
#include <netedit/changes/GNEChange_Attribute.h>

#include "GNECrossing.h"
#include "GNEJunction.h"
#include "GNEEdge.h"


// ===========================================================================
// method definitions
// ===========================================================================
GNECrossing::GNECrossing(GNEJunction* parentJunction, NBNode::Crossing* crossing) :
    GNENetElement(parentJunction->getNet(), crossing->id, GLO_CROSSING, SUMO_TAG_CROSSING),
    myParentJunction(parentJunction),
    myCrossing(crossing),
    myForceDrawCustomShape(crossing->customShape.size() > 0) {
    // Update geometry
    updateGeometry();
}


GNECrossing::~GNECrossing() {}


void
GNECrossing::updateGeometry() {
    // Clear Shape rotations and segments
    myShapeRotations.clear();
    myShapeLengths.clear();
    // only rebuild shape if junction's shape isn't in Buuble mode
    if (myParentJunction->getNBNode()->getShape().size() > 0) {
        // Obtain segments of size and calculate it
        PositionVector shape = myForceDrawCustomShape ?  myCrossing->customShape : myCrossing->shape;
        int segments = (int)shape.size() - 1;
        if (segments >= 0) {
            myShapeRotations.reserve(segments);
            myShapeLengths.reserve(segments);
            for (int i = 0; i < segments; ++i) {
                const Position& f = shape[i];
                const Position& s = shape[i + 1];
                myShapeLengths.push_back(f.distanceTo2D(s));
                myShapeRotations.push_back((double) atan2((s.x() - f.x()), (f.y() - s.y())) * (double) 180.0 / (double)M_PI);
            }
        }
    }
}


GNEJunction*
GNECrossing::getParentJunction() const {
    return myParentJunction;
}


NBNode::Crossing*
GNECrossing::getNBCrossing() const {
    return myCrossing;
}


void
GNECrossing::drawGL(const GUIVisualizationSettings& s) const {
    // only draw if option drawCrossingsAndWalkingareas is enabled and size of shape is greather than 0 and zoom is close enough
    if (s.drawCrossingsAndWalkingareas 
            && myShapeRotations.size() > 0 && myShapeLengths.size() > 0
            && s.scale > 3.0) {
        if (s.editMode != GNE_MODE_TLS) {
            // first declare what shape will be drawed
            PositionVector shape = myForceDrawCustomShape ?  myCrossing->customShape : myCrossing->shape;
            // push first draw matrix
            glPushMatrix();
            // push name
            glPushName(getGlID());
            // must draw on top of junction
            glTranslated(0, 0, GLO_JUNCTION + 0.1);
            // set color depending of selection and priority
            if (isAttributeCarrierSelected()) {
                glColor3d(0.118, 0.565, 1.000);
            } else if (!myCrossing->valid) {
                glColor3d(1.0, 0.1, 0.1);
            } else if (myCrossing->priority) {
                glColor3d(0.9, 0.9, 0.9);
            } else {
                glColor3d(0.1, 0.1, 0.1);
            }
            // traslate to front
            glTranslated(0, 0, .2);
            // set default values
            double length = 0.5;
            double spacing = 1.0;
            double halfWidth = myCrossing->width * 0.5;
            // push second draw matrix
            glPushMatrix();
            // draw on top of of the white area between the rails
            glTranslated(0, 0, 0.1);
            for (int i = 0; i < (int)shape.size() - 1; ++i) {
                // push three draw matrix
                glPushMatrix();
                // translate and rotate
                glTranslated(shape[i].x(), shape[i].y(), 0.0);
                glRotated(myShapeRotations[i], 0, 0, 1);
                // draw crossing depending if isn't being drawn for selecting
                if(!s.drawForSelecting) {
                    for (double t = 0; t < myShapeLengths[i]; t += spacing) {
                        glBegin(GL_QUADS);
                        glVertex2d(-halfWidth, -t);
                        glVertex2d(-halfWidth, -t - length);
                        glVertex2d(halfWidth, -t - length);
                        glVertex2d(halfWidth, -t);
                        glEnd();
                    }
                } else {
                    // only draw a single rectangle if it's being drawn only for selecting
                    glBegin(GL_QUADS);
                    glVertex2d(-halfWidth, 0);
                    glVertex2d(-halfWidth, -myShapeLengths.back());
                    glVertex2d(halfWidth, -myShapeLengths.back());
                    glVertex2d(halfWidth, 0);
                    glEnd();
                }
                // pop three draw matrix
                glPopMatrix();
            }
            // XXX draw junction index / tls index
            // pop second draw matrix
            glPopMatrix();
            // traslate to back
            glTranslated(0, 0, -.2);
            // pop name
            glPopName();
            // pop draw matrix
            glPopMatrix();
        }
        // link indices must be drawn in all edit modes if isn't being drawn for selecting
        if (s.drawLinkTLIndex.show && !s.drawForSelecting) {
            drawTLSLinkNo(s);
        }
        // check if dotted contour has to be drawn
        if(!s.drawForSelecting && (myNet->getViewNet()->getACUnderCursor() == this)) {
            GLHelper::drawShapeDottedContour(getType(), myCrossing->shape, myCrossing->width * 0.5);
        }
    }
}

void
GNECrossing::drawTLSLinkNo(const GUIVisualizationSettings& s) const {
    glPushMatrix();
    glTranslated(0, 0, GLO_JUNCTION + 0.5);
    PositionVector shape = myCrossing->shape;
    shape.extrapolate(0.5); // draw on top of the walking area
    int linkNo = myCrossing->tlLinkIndex;
    int linkNo2 = myCrossing->tlLinkIndex2 > 0 ? myCrossing->tlLinkIndex2 : linkNo;
    GLHelper::drawTextAtEnd(toString(linkNo2), shape, 0, s.drawLinkTLIndex.size, s.drawLinkTLIndex.color);
    GLHelper::drawTextAtEnd(toString(linkNo), shape.reverse(), 0, s.drawLinkTLIndex.size, s.drawLinkTLIndex.color);
    glPopMatrix();
}



GUIGLObjectPopupMenu*
GNECrossing::getPopUpMenu(GUIMainWindow& app, GUISUMOAbstractView& parent) {
    GUIGLObjectPopupMenu* ret = new GUIGLObjectPopupMenu(app, parent, *this);
    buildPopupHeader(ret, app);
    buildCenterPopupEntry(ret);
    buildNameCopyPopupEntry(ret);
    // build selection and show parameters menu
    buildSelectionPopupEntry(ret);
    buildShowParamsPopupEntry(ret);
    // build position copy entry
    buildPositionCopyEntry(ret, false);
    // create menu commands
    FXMenuCommand* mcCustomShape = new FXMenuCommand(ret, "Set custom crossing shape", 0, &parent, MID_GNE_CROSSING_EDIT_SHAPE);
    // check if menu commands has to be disabled
    EditMode editMode = myNet->getViewNet()->getCurrentEditMode();
    const bool wrongMode = (editMode == GNE_MODE_CONNECT || editMode == GNE_MODE_TLS || editMode == GNE_MODE_CREATE_EDGE);
    if (wrongMode) {
        mcCustomShape->disable();
    }
    return ret;
}


Boundary
GNECrossing::getCenteringBoundary() const {
    Boundary b = myCrossing->shape.getBoxBoundary();
    b.grow(10);
    return b;
}


std::string
GNECrossing::getAttribute(SumoXMLAttr key) const {
    switch (key) {
        case SUMO_ATTR_ID:
            return getMicrosimID();
        case SUMO_ATTR_WIDTH:
            return toString(myCrossing->customWidth);
        case SUMO_ATTR_PRIORITY:
            return myCrossing->priority ? "true" : "false";
        case SUMO_ATTR_EDGES:
            return toString(myCrossing->edges);
        case SUMO_ATTR_TLLINKINDEX:
            return toString(myCrossing->customTLIndex);
        case SUMO_ATTR_TLLINKINDEX2:
            return toString(myCrossing->customTLIndex2);
        case SUMO_ATTR_CUSTOMSHAPE:
            return toString(myCrossing->customShape);
        case GNE_ATTR_SELECTED:
            return toString(isAttributeCarrierSelected());
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


void
GNECrossing::setAttribute(SumoXMLAttr key, const std::string& value, GNEUndoList* undoList) {
    if (value == getAttribute(key)) {
        return; //avoid needless changes, later logic relies on the fact that attributes have changed
    }
    switch (key) {
        case SUMO_ATTR_ID:
            throw InvalidArgument("Modifying attribute '" + toString(key) + "' of " + toString(getTag()) + " isn't allowed");
        case SUMO_ATTR_EDGES:
        case SUMO_ATTR_WIDTH:
        case SUMO_ATTR_PRIORITY:
        case SUMO_ATTR_TLLINKINDEX:
        case SUMO_ATTR_TLLINKINDEX2:
        case SUMO_ATTR_CUSTOMSHAPE:
        case GNE_ATTR_SELECTED:
            undoList->add(new GNEChange_Attribute(this, key, value), true);
            break;
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNECrossing::isValid(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case SUMO_ATTR_ID:
            return false;
        case SUMO_ATTR_EDGES:
            if (canParse<std::vector<GNEEdge*> >(myNet, value, false)) {
                // parse edges and save their IDs in a set
                std::vector<GNEEdge*> parsedEdges = parse<std::vector<GNEEdge*> >(myNet, value);
                EdgeVector nbEdges;
                for (auto i : parsedEdges) {
                    nbEdges.push_back(i->getNBEdge());
                }
                std::sort(nbEdges.begin(), nbEdges.end());
                // 
                EdgeVector originalEdges = myCrossing->edges;
                std::sort(originalEdges.begin(), originalEdges.end());
                // return true if we're setting the same edges
                if(toString(nbEdges) == toString(originalEdges)) {
                    return true;
                } else {
                    return !myParentJunction->getNBNode()->checkCrossingDuplicated(nbEdges);
                }
            } else {
                return false;
            }
        case SUMO_ATTR_WIDTH:
            return canParse<double>(value) && ((parse<double>(value) > 0) || (parse<double>(value) == -1)); // kann NICHT 0 sein, oder -1 (bedeutet default)
        case SUMO_ATTR_PRIORITY:
            return canParse<bool>(value);
        case SUMO_ATTR_TLLINKINDEX:
        case SUMO_ATTR_TLLINKINDEX2:
            return (myCrossing->tlID != "" && canParse<int>(value) 
                    && ((parse<double>(value) > 0) || ((parse<double>(value) == -1) && (key == SUMO_ATTR_TLLINKINDEX2)))  // kann 0 sein (oder -1 -> es ist nicht verwendet)
                    && myParentJunction->getNBNode()->getControllingTLS().size() > 0
                    && (*myParentJunction->getNBNode()->getControllingTLS().begin())->getMaxValidIndex() >= parse<int>(value));
        case SUMO_ATTR_CUSTOMSHAPE: {
            bool ok = true;
            PositionVector shape = GeomConvHelper::parseShapeReporting(value, "user-supplied shape", 0, ok, true);
            return ok;
        }
        case GNE_ATTR_SELECTED:
            return canParse<bool>(value);
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNECrossing::checkEdgeBelong(GNEEdge* edge) const {
    if (std::find(myCrossing->edges.begin(), myCrossing->edges.end(), edge->getNBEdge()) !=  myCrossing->edges.end()) {
        return true;
    } else {
        return false;
    }
}


bool
GNECrossing::checkEdgeBelong(const std::vector<GNEEdge*>& edges) const {
    for (std::vector<GNEEdge*>::const_iterator i = edges.begin(); i != edges.end(); i++) {
        if (checkEdgeBelong(*i)) {
            return true;
        }
    }
    return false;
}

// ===========================================================================
// private
// ===========================================================================

void
GNECrossing::setAttribute(SumoXMLAttr key, const std::string& value) {
    switch (key) {
        case SUMO_ATTR_ID:
            throw InvalidArgument("Modifying attribute '" + toString(key) + "' of " + toString(getTag()) + " isn't allowed");
        case SUMO_ATTR_EDGES: {
            // obtain GNEEdges
            std::vector<GNEEdge*> edges = parse<std::vector<GNEEdge*> >(myNet, value);
            // remove NBEdges of crossing
            myCrossing->edges.clear();
            // set NBEdge of every GNEEdge into Crossing Edges
            for (auto i : edges) {
                myCrossing->edges.push_back(i->getNBEdge());
            }
            // sort new edges
            std::sort(myCrossing->edges.begin(), myCrossing->edges.end());
            // update geometry of parent junction
            myParentJunction->updateGeometry();
            break;
        }
        case SUMO_ATTR_WIDTH:
            // Change width an refresh element
            myCrossing->customWidth = parse<double>(value);
            myNet->refreshElement(this);
            break;
        case SUMO_ATTR_PRIORITY:
            myCrossing->priority = parse<bool>(value);
            break;
        case SUMO_ATTR_TLLINKINDEX:
            myCrossing->customTLIndex = parse<int>(value);
            break;
        case SUMO_ATTR_TLLINKINDEX2:
            myCrossing->customTLIndex2 = parse<int>(value);
            break;
        case SUMO_ATTR_CUSTOMSHAPE: {
            bool ok;
            myCrossing->customShape = GeomConvHelper::parseShapeReporting(value, "user-supplied shape", 0, ok, true);
            // Check if custom shaped has to be drawn
            myForceDrawCustomShape = myCrossing->customShape.size() > 0;
            updateGeometry();
            myNet->refreshElement(this);
            break;
        }
        case GNE_ATTR_SELECTED:
            if(parse<bool>(value)) {
                selectAttributeCarrier();
            } else {
                unselectAttributeCarrier();
            }
            break;
        default:
            throw InvalidArgument(toString(getTag()) + " doesn't have an attribute of type '" + toString(key) + "'");
    }
    // After setting attribute always update Geometry
    updateGeometry();
}


void 
GNECrossing::mouseOverObject(const GUIVisualizationSettings& s) const {

}

/****************************************************************************/
