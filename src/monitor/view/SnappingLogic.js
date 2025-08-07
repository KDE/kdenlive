/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Get snapped position for moving a point
function getSnappedPoint(position, scalex, scaley, gridH, gridV) {
    var deltax = Math.round(position.x / scalex)
    var deltay = Math.round(position.y / scaley)
    deltax = Math.round(deltax / gridH) * gridH
    deltay = Math.round(deltay / gridV) * gridV
    return Qt.point(deltax * scalex, deltay * scaley)
}

// Get the rotated bounding rectangle for the frame
function getRotatedBoundingRect(frameRect, rotationAngle) {
    if (Math.abs(rotationAngle) < 0.1) {
        return frameRect;
    }

    const angleRad = rotationAngle * Math.PI / 180.0;
    const absCos = Math.abs(Math.cos(angleRad));
    const absSin = Math.abs(Math.sin(angleRad));

    const originalWidth = frameRect.width;
    const originalHeight = frameRect.height;

    const rotatedWidth = originalWidth * absCos + originalHeight * absSin;
    const rotatedHeight = originalWidth * absSin + originalHeight * absCos;

    const centerX = frameRect.x + originalWidth / 2;
    const centerY = frameRect.y + originalHeight / 2;

    const x = centerX - rotatedWidth / 2;
    const y = centerY - rotatedHeight / 2;

    return Qt.rect(x, y, rotatedWidth, rotatedHeight);
}

// Get snapped resize position for a frame based on handle type
function getSnappedResizeRect(frameRect, rotationAngle, handleType, scalex, scaley, gridH, gridV) {
    if (Math.abs(rotationAngle) > 0.1) {
        // todo: not implemented yet
        return frameRect
    }
    
    // Convert frameRect to grid coordinates
    var gridX = frameRect.x / scalex
    var gridY = frameRect.y / scaley
    var gridWidth = frameRect.width / scalex
    var gridHeight = frameRect.height / scaley
    
    var snapTolerance = Math.max(gridH, gridV) * 0.3
    
    var adjustedFrame = Qt.rect(gridX, gridY, gridWidth, gridHeight)
    
    // Determine which edges to snap based on handle type
    var snapLeft = handleType.includes("LEFT")
    var snapRight = handleType.includes("RIGHT") 
    var snapTop = handleType.includes("TOP")
    var snapBottom = handleType.includes("BOTTOM")
    
    // Snap horizontal edges
    if (snapLeft) {
        var leftEdge = gridX
        var nearestLeftGrid = Math.round(leftEdge / gridH) * gridH
        var leftDist = Math.abs(leftEdge - nearestLeftGrid)
        if (leftDist <= snapTolerance) {
            var deltaX = nearestLeftGrid - leftEdge
            adjustedFrame.x = nearestLeftGrid
            adjustedFrame.width = gridWidth - deltaX  // Expand/shrink from left
        }
    } else if (snapRight) {
        var rightEdge = gridX + gridWidth
        var nearestRightGrid = Math.round(rightEdge / gridH) * gridH
        var rightDist = Math.abs(rightEdge - nearestRightGrid)
        if (rightDist <= snapTolerance) {
            adjustedFrame.width = nearestRightGrid - gridX  // Expand/shrink from right
        }
    }
    
    // Snap vertical edges
    if (snapTop) {
        var topEdge = gridY
        var nearestTopGrid = Math.round(topEdge / gridV) * gridV
        var topDist = Math.abs(topEdge - nearestTopGrid)
        if (topDist <= snapTolerance) {
            var deltaY = nearestTopGrid - topEdge
            adjustedFrame.y = nearestTopGrid
            adjustedFrame.height = gridHeight - deltaY  // Expand/shrink from top
        }
    } else if (snapBottom) {
        var bottomEdge = gridY + gridHeight
        var nearestBottomGrid = Math.round(bottomEdge / gridV) * gridV
        var bottomDist = Math.abs(bottomEdge - nearestBottomGrid)
        if (bottomDist <= snapTolerance) {
            adjustedFrame.height = nearestBottomGrid - gridY  // Expand/shrink from bottom
        }
    }
    
    // Convert back to screen coordinates
    return Qt.rect(
        adjustedFrame.x * scalex,
        adjustedFrame.y * scaley, 
        adjustedFrame.width * scalex,
        adjustedFrame.height * scaley
    )
}

// Get snapped position for moving a frame
function getSnappedRect(frameRect, rotationAngle, scalex, scaley, gridH, gridV) {
    // Continue with the bounded rectangle of frameRect to support rotation
    var boundingRect = getRotatedBoundingRect(frameRect, rotationAngle || 0)
    
    // Convert bounding rect to grid coordinates
    var gridX = boundingRect.x / scalex
    var gridY = boundingRect.y / scaley
    var gridWidth = boundingRect.width / scalex
    var gridHeight = boundingRect.height / scaley

    // Get positions of the frame's edges
    var leftEdge = gridX
    var rightEdge = gridX + gridWidth
    var topEdge = gridY
    var bottomEdge = gridY + gridHeight
    
    // Find nearest grid lines for each edge
    var nearestLeftGrid = Math.round(leftEdge / gridH) * gridH
    var nearestRightGrid = Math.round(rightEdge / gridH) * gridH
    var nearestTopGrid = Math.round(topEdge / gridV) * gridV
    var nearestBottomGrid = Math.round(bottomEdge / gridV) * gridV
    
    // Calculate distances (in grid units)
    var leftDist = Math.abs(leftEdge - nearestLeftGrid)
    var rightDist = Math.abs(rightEdge - nearestRightGrid)
    var topDist = Math.abs(topEdge - nearestTopGrid)
    var bottomDist = Math.abs(bottomEdge - nearestBottomGrid)
    
    // Snap tolerance (in grid units)
    var snapTolerance = Math.max(gridH, gridV) * 0.3
    
    var adjustedX = gridX
    var adjustedY = gridY
    
    // Find closest horizontal edge
    var minHorizontalDist = Math.min(leftDist, rightDist)
    if (minHorizontalDist <= snapTolerance) {
        if (leftDist <= rightDist) {
            // Snap left edge
            adjustedX = nearestLeftGrid
        } else {
            // Snap right edge  
            adjustedX = nearestRightGrid - gridWidth
        }
    }
    
    // Find closest vertical edge
    var minVerticalDist = Math.min(topDist, bottomDist)
    if (minVerticalDist <= snapTolerance) {
        if (topDist <= bottomDist) {
            // Snap top edge
            adjustedY = nearestTopGrid
        } else {
            // Snap bottom edge
            adjustedY = nearestBottomGrid - gridHeight
        }
    }
    
    // Calculate the offset between original and snapped bounding rect
    var offsetX = (adjustedX - gridX) * scalex
    var offsetY = (adjustedY - gridY) * scaley
    
    // Apply the offset to the original frame position
    return Qt.rect(frameRect.x + offsetX, frameRect.y + offsetY, frameRect.width, frameRect.height)
}
