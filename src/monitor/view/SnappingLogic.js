/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Calculate snap threshold based on grid size
// If the distance to the nearest grid line is less than the snap threshold, snap to the grid line.
function calculateSnapThreshold(gridH, gridV) {
    return Math.max(gridH, gridV) * 0.3
}

// Get snapped position for moving a point
function getSnappedPoint(point, gridH, gridV) {
    var origX = point.x
    var origY = point.y
    
    var snapThreshold = calculateSnapThreshold(gridH, gridV)
    
    // Find nearest grid positions
    var nearestGridX = Math.round(origX / gridH) * gridH
    var nearestGridY = Math.round(origY / gridV) * gridV
    
    // Calculate distances to nearest grid lines
    var distX = Math.abs(origX - nearestGridX)
    var distY = Math.abs(origY - nearestGridY)
    
    // Only snap if within threshold
    var snappedX = distX <= snapThreshold ? nearestGridX : origX
    var snappedY = distY <= snapThreshold ? nearestGridY : origY
    
    return Qt.point(snappedX, snappedY)
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
function getSnappedResizeRect(frameRect, rotationAngle, handleType, gridH, gridV) {
    if (Math.abs(rotationAngle) > 0.1) {
        // todo: not implemented yet
        return frameRect
    }
    
    var origX = frameRect.x
    var origY = frameRect.y
    var origWidth = frameRect.width
    var origHeight = frameRect.height
    
    var snapThreshold = calculateSnapThreshold(gridH, gridV)
    
    var adjustedFrame = Qt.rect(origX, origY, origWidth, origHeight)
    
    // Determine which edges to snap based on handle type
    var snapLeft = handleType.includes("LEFT")
    var snapRight = handleType.includes("RIGHT") 
    var snapTop = handleType.includes("TOP")
    var snapBottom = handleType.includes("BOTTOM")

    // Calculate distances to nearest grid lines and snap only if within threshold
    
    // Snap horizontal edges
    if (snapLeft) {
        var leftEdge = origX
        var nearestLeftGrid = Math.round(leftEdge / gridH) * gridH
        var leftDist = Math.abs(leftEdge - nearestLeftGrid)
        if (leftDist <= snapThreshold) {
            var deltaX = nearestLeftGrid - leftEdge
            adjustedFrame.x = nearestLeftGrid
            adjustedFrame.width = origWidth - deltaX  // Expand/shrink from left
        }
    } else if (snapRight) {
        var rightEdge = origX + origWidth
        var nearestRightGrid = Math.round(rightEdge / gridH) * gridH
        var rightDist = Math.abs(rightEdge - nearestRightGrid)
        if (rightDist <= snapThreshold) {
            adjustedFrame.width = nearestRightGrid - origX  // Expand/shrink from right
        }
    }
    
    // Snap vertical edges
    if (snapTop) {
        var topEdge = origY
        var nearestTopGrid = Math.round(topEdge / gridV) * gridV
        var topDist = Math.abs(topEdge - nearestTopGrid)
        if (topDist <= snapThreshold) {
            var deltaY = nearestTopGrid - topEdge
            adjustedFrame.y = nearestTopGrid
            adjustedFrame.height = origHeight - deltaY  // Expand/shrink from top
        }
    } else if (snapBottom) {
        var bottomEdge = origY + origHeight
        var nearestBottomGrid = Math.round(bottomEdge / gridV) * gridV
        var bottomDist = Math.abs(bottomEdge - nearestBottomGrid)
        if (bottomDist <= snapThreshold) {
            adjustedFrame.height = nearestBottomGrid - origY  // Expand/shrink from bottom
        }
    }
    
    return adjustedFrame
}

// Get snapped position for moving a frame
function getSnappedRect(frameRect, rotationAngle, gridH, gridV) {
    // Continue with the bounded rectangle of frameRect to support rotation
    var boundingRect = getRotatedBoundingRect(frameRect, rotationAngle || 0)
    
    var origX = boundingRect.x
    var origY = boundingRect.y
    var origWidth = boundingRect.width
    var origHeight = boundingRect.height

    // Get positions of the frame's edges
    var leftEdge = origX
    var rightEdge = origX + origWidth
    var topEdge = origY
    var bottomEdge = origY + origHeight
    
    // Find nearest grid lines for each edge
    var nearestLeftGrid = Math.round(leftEdge / gridH) * gridH
    var nearestRightGrid = Math.round(rightEdge / gridH) * gridH
    var nearestTopGrid = Math.round(topEdge / gridV) * gridV
    var nearestBottomGrid = Math.round(bottomEdge / gridV) * gridV
    
    // Calculate distances to nearest grid lines
    var leftDist = Math.abs(leftEdge - nearestLeftGrid)
    var rightDist = Math.abs(rightEdge - nearestRightGrid)
    var topDist = Math.abs(topEdge - nearestTopGrid)
    var bottomDist = Math.abs(bottomEdge - nearestBottomGrid)
    
    var snapThreshold = calculateSnapThreshold(gridH, gridV)
    
    var adjustedX = origX
    var adjustedY = origY
    
    // Find closest horizontal edge
    var minHorizontalDist = Math.min(leftDist, rightDist)
    // Only snap if within threshold
    if (minHorizontalDist <= snapThreshold) {
        if (leftDist <= rightDist) {
            // Snap left edge
            adjustedX = nearestLeftGrid
        } else {
            // Snap right edge  
            adjustedX = nearestRightGrid - origWidth
        }
    }
    
    // Find closest vertical edge
    var minVerticalDist = Math.min(topDist, bottomDist)
    // Only snap if within threshold
    if (minVerticalDist <= snapThreshold) {
        if (topDist <= bottomDist) {
            // Snap top edge
            adjustedY = nearestTopGrid
        } else {
            // Snap bottom edge
            adjustedY = nearestBottomGrid - origHeight
        }
    }
    
    // Calculate the offset between original and snapped bounding rect
    var offsetX = adjustedX - origX
    var offsetY = adjustedY - origY
    
    // Apply the offset to the original frame position
    return Qt.rect(frameRect.x + offsetX, frameRect.y + offsetY, frameRect.width, frameRect.height)
}
