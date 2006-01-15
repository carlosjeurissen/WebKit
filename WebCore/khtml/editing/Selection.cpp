/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
  
#include "config.h"
#include "Selection.h"

#include "DocumentImpl.h"
#include "htmlediting.h"
#include "visible_position.h"
#include "visible_units.h"
#include <kxmlcore/Assertions.h>

namespace WebCore {

Selection::Selection()
    : m_affinity(DOWNSTREAM)
    , m_state(NONE)
    , m_baseIsFirst(true)
{
}

Selection::Selection(const Position &base, const Position &extent, EAffinity affinity)
    : m_base(base), m_extent(extent)
    , m_affinity(affinity)
    , m_state(NONE)
    , m_baseIsFirst(true)
{
    validate();
}

void Selection::clear()
{
    m_affinity = SEL_DEFAULT_AFFINITY;
    m_base.clear();
    m_extent.clear();
}

PassRefPtr<RangeImpl> Selection::toRange() const
{
    if (isNone())
        return 0;

    // Make sure we have an updated layout since this function is called
    // in the course of running edit commands which modify the DOM.
    // Failing to call this can result in equivalentXXXPosition calls returning
    // incorrect results.
    m_start.node()->getDocument()->updateLayout();

    Position s, e;
    if (isCaret()) {
        // If the selection is a caret, move the range start upstream. This helps us match
        // the conventions of text editors tested, which make style determinations based
        // on the character before the caret, if any. 
        s = m_start.upstream().equivalentRangeCompliantPosition();
        e = s;
    }
    else {
        // If the selection is a range, select the minimum range that encompasses the selection.
        // Again, this is to match the conventions of text editors tested, which make style 
        // determinations based on the first character of the selection. 
        // For instance, this operation helps to make sure that the "X" selected below is the 
        // only thing selected. The range should not be allowed to "leak" out to the end of the 
        // previous text node, or to the beginning of the next text node, each of which has a 
        // different style.
        // 
        // On a treasure map, <b>X</b> marks the spot.
        //                       ^ selected
        //
        ASSERT(isRange());
        s = m_start.downstream();
        e = m_end.upstream();
        if (RangeImpl::compareBoundaryPoints(s.node(), s.offset(), e.node(), e.offset()) > 0) {
            // Make sure the start is before the end.
            // The end can wind up before the start if collapsed whitespace is the only thing selected.
            Position tmp = s;
            s = e;
            e = tmp;
        }
        s = s.equivalentRangeCompliantPosition();
        e = e.equivalentRangeCompliantPosition();
    }

    int exceptionCode = 0;
    PassRefPtr<RangeImpl> result(new RangeImpl(s.node()->getDocument()));
    result->setStart(s.node(), s.offset(), exceptionCode);
    if (exceptionCode) {
        ERROR("Exception setting Range start from Selection: %d", exceptionCode);
        return 0;
    }
    result->setEnd(e.node(), e.offset(), exceptionCode);
    if (exceptionCode) {
        ERROR("Exception setting Range end from Selection: %d", exceptionCode);
        return 0;
    }
    return result;
}

void Selection::validate(ETextGranularity granularity)
{
    // Move the selection to rendered positions, if possible.
    Position originalBase(m_base);
    bool baseAndExtentEqual = m_base == m_extent;
    if (m_base.isNotNull()) {
        m_base = VisiblePosition(m_base, m_affinity).deepEquivalent();
        if (baseAndExtentEqual)
            m_extent = m_base;
    }
    if (m_extent.isNotNull() && !baseAndExtentEqual) {
        m_extent = VisiblePosition(m_extent, m_affinity).deepEquivalent();
    }

    // Make sure we do not have a dangling start or end
    if (m_base.isNull() && m_extent.isNull()) {
        // Move the position to the enclosingBlockFlowElement of the original base, if possible.
        // This has the effect of flashing the caret somewhere when a rendered position for
        // the base and extent cannot be found.
        if (originalBase.isNotNull()) {
            Position pos(originalBase.node()->enclosingBlockFlowElement(), 0);
            m_base = pos;
            m_extent = pos;
        }
        else {
            // We have no position to work with. See if the BODY element of the page
            // is contentEditable. If it is, put the caret there.
            //NodeImpl *node = document()
            m_start.clear();
            m_end.clear();
        }
        m_baseIsFirst = true;
    } else if (m_base.isNull()) {
        m_base = m_extent;
        m_baseIsFirst = true;
    } else if (m_extent.isNull()) {
        m_extent = m_base;
        m_baseIsFirst = true;
    } else {
        m_baseIsFirst = RangeImpl::compareBoundaryPoints(m_base.node(), m_base.offset(), m_extent.node(), m_extent.offset()) <= 0;
    }

    if (m_baseIsFirst) {
        m_start = m_base;
        m_end = m_extent;
    } else {
        m_start = m_extent;
        m_end = m_base;
    }
    
    // Expand the selection if requested.
    switch (granularity) {
        case CHARACTER:
            // Don't do any expansion.
            break;
        case WORD: {
            // General case: Select the word the caret is positioned inside of, or at the start of (RightWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a soft-wrapped line or the last word in
            // the document, select that last word (LeftWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a paragraph, select from the the end of the
            // last word to the line break (also RightWordIfOnBoundary);
            VisiblePosition start = VisiblePosition(m_start, m_affinity);
            VisiblePosition end   = VisiblePosition(m_end, m_affinity);
            EWordSide side = RightWordIfOnBoundary;
            if (isEndOfDocument(start) || (isEndOfLine(start) && !isStartOfLine(start) && !isEndOfParagraph(start)))
                side = LeftWordIfOnBoundary;
            m_start = startOfWord(start, side).deepEquivalent();
            side = RightWordIfOnBoundary;
            if (isEndOfDocument(end) || (isEndOfLine(end) && !isStartOfLine(end) && !isEndOfParagraph(end)))
                side = LeftWordIfOnBoundary;
            m_end = endOfWord(end, side).deepEquivalent();
            
            break;
            }
        case LINE: {
            m_start = startOfLine(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            VisiblePosition end = endOfLine(VisiblePosition(m_end, m_affinity));
            // If the end of this line is at the end of a paragraph, include the space 
            // after the end of the line in the selection.
            if (isEndOfParagraph(end)) {
                VisiblePosition next = end.next();
                if (next.isNotNull())
                    end = next;
            }
            m_end = end.deepEquivalent();
            break;
        }
        case LINE_BOUNDARY:
            m_start = startOfLine(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfLine(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case PARAGRAPH: {
            VisiblePosition pos(m_start, m_affinity);
            if (isStartOfLine(pos) && isEndOfDocument(pos))
                pos = pos.previous();
            m_start = startOfParagraph(pos).deepEquivalent();
            VisiblePosition visibleParagraphEnd = endOfParagraph(VisiblePosition(m_end, m_affinity));
            // Include the space after the end of the paragraph in the selection.
            VisiblePosition startOfNextParagraph = visibleParagraphEnd.next();
            m_end = startOfNextParagraph.isNotNull() ? startOfNextParagraph.deepEquivalent() : visibleParagraphEnd.deepEquivalent();
            break;
        }
        case DOCUMENT_BOUNDARY:
            m_start = startOfDocument(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfDocument(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case PARAGRAPH_BOUNDARY:
            m_start = startOfParagraph(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfParagraph(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
    }
    
    adjustForEditableContent();

    // adjust the state
    if (m_start.isNull()) {
        ASSERT(m_end.isNull());
        m_state = NONE;

        // enforce downstream affinity if not caret, as affinity only
        // makes sense for caret
        m_affinity = DOWNSTREAM;
    } else if (m_start == m_end || m_start.upstream() == m_end.upstream()) {
        m_state = CARET;
    } else {
        m_state = RANGE;

        // enforce downstream affinity if not caret, as affinity only
        // makes sense for caret
        m_affinity = DOWNSTREAM;

        // "Constrain" the selection to be the smallest equivalent range of nodes.
        // This is a somewhat arbitrary choice, but experience shows that it is
        // useful to make to make the selection "canonical" (if only for
        // purposes of comparing selections). This is an ideal point of the code
        // to do this operation, since all selection changes that result in a RANGE 
        // come through here before anyone uses it.
        m_start = m_start.downstream();
        m_end = m_end.upstream();
    }
}

void Selection::adjustForEditableContent()
{
    if (m_base.isNull())
        return;

    NodeImpl *baseRoot = m_base.node()->rootEditableElement();
    NodeImpl *startRoot = m_start.node()->rootEditableElement();
    NodeImpl *endRoot = m_end.node()->rootEditableElement();
    
    // The base, start and end are all in the same region.  No adjustment necessary.
    if (baseRoot == startRoot && baseRoot == endRoot)
        return;
    
    // The selection is based in an editable area.  Keep both sides from reaching outside that area.
    if (baseRoot) {
        // If the start is outside the base's editable root, cap it at the start of that editable root.
        if (baseRoot != startRoot) {
            VisiblePosition first(Position(baseRoot, 0));
            m_start = first.deepEquivalent();
        }
        // If the end is outside the base's editable root, cap it at the end of that editable root.
        if (baseRoot != endRoot) {
            VisiblePosition last(Position(baseRoot, maxDeepOffset(baseRoot)));
            m_end = last.deepEquivalent();
        }
    // The selection is based outside editable content.  Keep both sides from reaching into editable content.
    } else {
        // The selection ends in editable content, move backward until non-editable content is reached.
        if (endRoot) {
            VisiblePosition previous;
            do {
                previous = VisiblePosition(Position(endRoot, 0)).previous();
                endRoot = previous.deepEquivalent().node()->rootEditableElement();
            } while (endRoot);
            
            ASSERT(!previous.isNull());
            m_end = previous.deepEquivalent();
        }
        // The selection starts in editable content, move forward until non-editable content is reached.
        if (startRoot) {
            VisiblePosition next;
            do {
                next = VisiblePosition(Position(startRoot, maxDeepOffset(startRoot))).next();
                startRoot = next.deepEquivalent().node()->rootEditableElement();
            } while (startRoot);
            
            ASSERT(!next.isNull());
            m_start = next.deepEquivalent();
        }
    }
    
    // Correct the extent if necessary.
    if (baseRoot != m_extent.node()->rootEditableElement())
        m_extent = m_baseIsFirst ? m_end : m_start;
}

void Selection::debugPosition() const
{
    if (!m_start.node())
        return;

    fprintf(stderr, "Selection =================\n");

    if (m_start == m_end) {
        Position pos = m_start;
        fprintf(stderr, "pos:        %s %p:%d\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
    } else {
        Position pos = m_start;
        fprintf(stderr, "start:      %s %p:%d\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "-----------------------------------\n");
        pos = m_end;
        fprintf(stderr, "end:        %s %p:%d\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "-----------------------------------\n");
    }

    fprintf(stderr, "================================\n");
}

} // namespace DOM
