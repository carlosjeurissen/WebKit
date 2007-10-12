/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef SVGInlineTextBox_h
#define SVGInlineTextBox_h

#include "InlineTextBox.h"

#if ENABLE(SVG)

namespace WebCore {

    class SVGChar;
    class SVGRootInlineBox;

    class SVGInlineTextBox : public InlineTextBox {
    public:
        SVGInlineTextBox(RenderObject* obj);

        virtual int selectionTop();
        virtual int selectionHeight();

        virtual int offsetForPosition(int x, bool includePartialGlyphs = true) const;
        virtual int positionForOffset(int offset) const;

        virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty);
        virtual IntRect selectionRect(int absx, int absy, int startPos, int endPos);

        SVGRootInlineBox* svgRootInlineBox() const;
 
    protected:
        friend class RenderSVGInlineText;
        bool svgCharacterHitsPosition(int x, int y, int& offset) const;

    private:
        SVGChar* closestCharacterToPosition(int x, int y, int& offset) const;
    };

} // namespace WebCore

#endif
#endif // SVGInlineTextBox_h
