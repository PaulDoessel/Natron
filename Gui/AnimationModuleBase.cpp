/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2016 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****


#include "AnimationModuleBase.h"

#include "Engine/KnobTypes.h"
#include "Engine/Node.h"

#include "Gui/AnimationModuleSelectionModel.h"
#include "Gui/AnimationModuleUndoRedo.h"
#include "Gui/KnobAnim.h"
#include "Gui/KnobUndoCommand.h"
#include "Gui/NodeAnim.h"
#include "Gui/TableItemAnim.h"

NATRON_NAMESPACE_ENTER;

struct AnimationModuleBasePrivate
{
    AnimItemDimViewKeyFramesMap keyframesClipboard;

    AnimationModuleBasePrivate()
    : keyframesClipboard()
    {

    }
};

AnimationModuleBase::AnimationModuleBase()
: _imp(new AnimationModuleBasePrivate())
{

}

AnimationModuleBase::~AnimationModuleBase()
{

}

void
AnimationModuleBase::setCurrentSelection(const AnimItemDimViewKeyFramesMap &keys,
                                         const std::vector<TableItemAnimPtr>& selectedTableItems,
                                         const std::vector<NodeAnimPtr >& rangeNodes)
{
    getSelectionModel()->makeSelection(keys,
                                       selectedTableItems,
                                       rangeNodes,
                                       AnimationModuleSelectionModel::SelectionTypeAdd |
                                       AnimationModuleSelectionModel::SelectionTypeClear);
    refreshSelectionBboxAndUpdateView();
}

void
AnimationModuleBase::deleteSelectedKeyframes()
{
    AnimationModuleSelectionModelPtr selectionModel = getSelectionModel();
    if ( selectionModel->isEmpty() ) {
        return;
    }

    const AnimItemDimViewKeyFramesMap& selectedKeyframes = selectionModel->getCurrentKeyFramesSelection();


    pushUndoCommand(new RemoveKeysCommand(selectedKeyframes, shared_from_this()));
}




void
AnimationModuleBase::moveSelectedKeysAndNodes(double dt, double dv)
{
    AnimationModuleSelectionModelPtr selectionModel = getSelectionModel();
    if ( selectionModel->isEmpty() ) {
        return;
    }

    const AnimItemDimViewKeyFramesMap& selectedKeyframes = selectionModel->getCurrentKeyFramesSelection();
    const std::list<NodeAnimPtr>& selectedNodes = selectionModel->getCurrentNodesSelection();
    const std::list<TableItemAnimPtr>& selectedTableItems = selectionModel->getCurrentTableItemsSelection();
    
    // Constraint dt according to keyframe positions
    double maxLeft = INT_MIN;
    double maxRight = INT_MAX;



    for (AnimItemDimViewKeyFramesMap::const_iterator it = selectedKeyframes.begin(); it != selectedKeyframes.end(); ++it) {
        AnimItemBasePtr item = it->first.item;
        if (!item) {
            continue;
        }
        CurvePtr curve = item->getCurve(it->first.dim, it->first.view);
        if (!curve) {
            continue;
        }

        double epsilon = curve->areKeyFramesTimeClampedToIntegers() ? 1 : 1e-4;


        KeyFrame prevKey, nextKey;
        for (KeyFrameWithStringSet::const_iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {

            // Adjust maxRight so that it can move up to the next keyframe
            if ( curve->getNextKeyframeTime( it2->key.getTime(), &nextKey ) ) {


                // If this keyframe has a next keyframe, check if it is selected
                KeyFrameWithString k;
                k.key = nextKey;
                KeyFrameWithStringSet::const_iterator foundKey = it->second.find(k);
                if (foundKey == it->second.end()) {
                    // The next keyframe is not selected
                    double diff = nextKey.getTime() - it2->key.getTime() - epsilon;
                    maxRight = std::max( 0., std::min(diff, maxRight) );
                }

            }

            // Adjust maxLeft so that it can move up to the previous keyframe
            if ( curve->getPreviousKeyframeTime( it2->key.getTime(), &prevKey ) ) {

                // If this keyframe has a next keyframe, check if it is selected
                KeyFrameWithString k;
                k.key = prevKey;
                KeyFrameWithStringSet::const_iterator foundKey = it->second.find(k);
                if (foundKey == it->second.end()) {
                    // The next keyframe is not selected
                    double diff = prevKey.getTime() - it2->key.getTime() + epsilon;
                    maxLeft = std::min( 0., std::max(diff, maxLeft) );
                }
            }

        } // for all keyframes
    } // for all item/dim/view

    // Do the clamp
    dt = std::min(dt, maxRight);
    dt = std::max(dt, maxLeft);
    if (dt == 0 && dv == 0) {
        return;
    }

    pushUndoCommand(new WarpKeysCommand(selectedKeyframes, shared_from_this(), selectedNodes, selectedTableItems, dt, dv));

} // AnimationModuleBase::moveSelectedKeysAndNodes

void
AnimationModuleBase::trimReaderLeft(const NodeAnimPtr &reader,
                                double newFirstFrame)
{
    NodePtr node = reader->getInternalNode();

    KnobIntPtr firstFrameKnob = toKnobInt(node->getKnobByName(kReaderParamNameFirstFrame));
    KnobIntPtr lastFrameKnob = toKnobInt(node->getKnobByName(kReaderParamNameLastFrame));
    assert(lastFrameKnob);
    KnobIntPtr originalFrameRangeKnob = toKnobInt( node->getKnobByName(kReaderParamNameOriginalFrameRange));
    assert(originalFrameRangeKnob);


    int firstFrame = firstFrameKnob->getValue();
    int lastFrame = lastFrameKnob->getValue();
    int originalFirstFrame = originalFrameRangeKnob->getValue();

    newFirstFrame = std::max( (double)newFirstFrame, (double)originalFirstFrame );
    newFirstFrame = std::min( (double)lastFrame, newFirstFrame );
    if (newFirstFrame == firstFrame) {
        return;
    }

    pushUndoCommand( new KnobUndoCommand<int>(firstFrameKnob, firstFrame, (int)newFirstFrame, DimIdx(0), ViewSetSpec(0), eValueChangedReasonUserEdited, tr("Trim Left")));
}

void
AnimationModuleBase::trimReaderRight(const NodeAnimPtr &reader,
                                 double newLastFrame)
{
    NodePtr node = reader->getInternalNode();

    KnobIntPtr firstFrameKnob = toKnobInt(node->getKnobByName(kReaderParamNameFirstFrame));
    KnobIntPtr lastFrameKnob = toKnobInt(node->getKnobByName(kReaderParamNameLastFrame));
    assert(lastFrameKnob);
    KnobIntPtr originalFrameRangeKnob = toKnobInt( node->getKnobByName(kReaderParamNameOriginalFrameRange));
    assert(originalFrameRangeKnob);

    int firstFrame = firstFrameKnob->getValue();
    int lastFrame = lastFrameKnob->getValue();
    int originalLastFrame = originalFrameRangeKnob->getValue(DimIdx(1));

    newLastFrame = std::min( (double)newLastFrame, (double)originalLastFrame );
    newLastFrame = std::max( (double)firstFrame, newLastFrame );
    if (newLastFrame == lastFrame) {
        return;
    }

    pushUndoCommand( new KnobUndoCommand<int>(lastFrameKnob, lastFrame, (int)newLastFrame, DimIdx(0), ViewSetSpec(0), eValueChangedReasonUserEdited, tr("Trim Right")));
}

bool
AnimationModuleBase::canSlipReader(const NodeAnimPtr &reader) const
{
    NodePtr node = reader->getInternalNode();

    KnobIntPtr firstFrameKnob = toKnobInt(node->getKnobByName(kReaderParamNameFirstFrame));
    KnobIntPtr lastFrameKnob = toKnobInt(node->getKnobByName(kReaderParamNameLastFrame));
    assert(lastFrameKnob);
    KnobIntPtr originalFrameRangeKnob = toKnobInt( node->getKnobByName(kReaderParamNameOriginalFrameRange));
    assert(originalFrameRangeKnob);


    ///Slipping means moving the timeOffset parameter by dt and moving firstFrame and lastFrame by -dt
    ///dt is clamped (firstFrame-originalFirstFrame) and (originalLastFrame-lastFrame)

    int currentFirstFrame = firstFrameKnob->getValue();
    int currentLastFrame = lastFrameKnob->getValue();
    int originalFirstFrame = originalFrameRangeKnob->getValue(DimIdx(0));
    int originalLastFrame = originalFrameRangeKnob->getValue(DimIdx(1));

    if ( ( (currentFirstFrame - originalFirstFrame) == 0 ) && ( (currentLastFrame - originalLastFrame) == 0 ) ) {
        return false;
    }

    return true;
}

void
AnimationModuleBase::slipReader(const NodeAnimPtr &reader,
                            double dt)
{
    NodePtr node = reader->getInternalNode();

    KnobIntPtr firstFrameKnob = toKnobInt(node->getKnobByName(kReaderParamNameFirstFrame));
    KnobIntPtr lastFrameKnob = toKnobInt(node->getKnobByName(kReaderParamNameLastFrame));
    assert(lastFrameKnob);
    KnobIntPtr originalFrameRangeKnob = toKnobInt( node->getKnobByName(kReaderParamNameOriginalFrameRange));
    assert(originalFrameRangeKnob);
    KnobIntPtr timeOffsetKnob = toKnobInt( node->getKnobByName(kReaderParamNameTimeOffset));
    assert(timeOffsetKnob);


    ///Slipping means moving the timeOffset parameter by dt and moving firstFrame and lastFrame by -dt
    ///dt is clamped (firstFrame-originalFirstFrame) and (originalLastFrame-lastFrame)

    int currentFirstFrame = firstFrameKnob->getValue();
    int currentLastFrame = lastFrameKnob->getValue();
    int originalFirstFrame = originalFrameRangeKnob->getValue(DimIdx(0));
    int originalLastFrame = originalFrameRangeKnob->getValue(DimIdx(1));

    dt = std::min( dt, (double)(currentFirstFrame - originalFirstFrame) );
    dt = std::max( dt, (double)(currentLastFrame - originalLastFrame) );

    if (dt != 0) {
        EffectInstancePtr effect = node->getEffectInstance();
        effect->beginMultipleEdits(tr("Slip Reader").toStdString());
        firstFrameKnob->setValue(firstFrameKnob->getValue() - dt, ViewSetSpec(0), DimIdx(0), eValueChangedReasonUserEdited);
        lastFrameKnob->setValue(lastFrameKnob->getValue() - dt, ViewSetSpec(0), DimIdx(0), eValueChangedReasonUserEdited);
        timeOffsetKnob->setValue(timeOffsetKnob->getValue() + dt, ViewSetSpec(0), DimIdx(0), eValueChangedReasonUserEdited);
        effect->endMultipleEdits();
    }
}

void
AnimationModuleBase::copySelectedKeys()
{
    AnimationModuleSelectionModelPtr selectionModel = getSelectionModel();
    if ( selectionModel->isEmpty() ) {
        return;
    }

    const AnimItemDimViewKeyFramesMap& selectedKeyframes = selectionModel->getCurrentKeyFramesSelection();
    
    _imp->keyframesClipboard = selectedKeyframes;
    
}


const AnimItemDimViewKeyFramesMap&
AnimationModuleBase::getKeyFramesClipBoard() const
{
    return _imp->keyframesClipboard;
}

void
AnimationModuleBase::pasteKeys(const AnimItemDimViewKeyFramesMap& keys, bool relative)
{
    if ( !keys.empty() ) {
        AnimationModuleSelectionModelPtr selectionModel = getSelectionModel();


        const AnimItemDimViewKeyFramesMap& keys = selectionModel->getCurrentKeyFramesSelection();

        if (keys.empty() || keys.size() > 1) {
            Dialogs::warningDialog(tr("Paste KeyFrame(s)").toStdString(), tr("You must select exactly one target item to perform this action").toStdString());
            return;
        }
        const AnimItemDimViewIndexID& targetID = keys.begin()->first;
        double time = 0;
        TimeLinePtr timeline = getTimeline();
        if (timeline) {
            time = timeline->currentFrame();
        }
#pragma message WARN("Fix it: user can copy animation on master keyframes of a knob, it only works for specific view/dim")
        pushUndoCommand(new PasteKeysCommand(keys, shared_from_this(), targetID.item, targetID.dim, targetID.view, relative, time));
    }
}

void
AnimationModuleBase::setSelectedKeysInterpolation(KeyframeTypeEnum keyType)
{
    AnimationModuleSelectionModelPtr selectionModel = getSelectionModel();
    const AnimItemDimViewKeyFramesMap& selectedKeyframes = selectionModel->getCurrentKeyFramesSelection();
    if (selectedKeyframes.empty()) {
        return;
    }
    pushUndoCommand(new SetKeysInterpolationCommand(selectedKeyframes, shared_from_this(), keyType, 0));

}

void
AnimationModuleBase::transformSelectedKeys(const Transform::Matrix3x3& transform)
{
    AnimationModuleSelectionModelPtr selectionModel = getSelectionModel();

    const AnimItemDimViewKeyFramesMap& selectedKeyframes = selectionModel->getCurrentKeyFramesSelection();
    if (selectedKeyframes.empty()) {
        return;
    }
    boost::scoped_ptr<Curve::AffineKeyFrameWarp> warp(new Curve::AffineKeyFrameWarp(transform));

    // Test the warp, if it cannot be applied, do not push an undo/redo command
    if (!WarpKeysCommand::testWarpOnKeys(selectedKeyframes, *warp)) {
        return;
    }

    // Try once to warp everything, if it doesn't work fail it now
    pushUndoCommand(new WarpKeysCommand(selectedKeyframes, shared_from_this(), transform));
}


NATRON_NAMESPACE_EXIT;