/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Handle type constants
var HandleTypes = {
    TOP_LEFT: "TOP_LEFT",
    TOP: "TOP", 
    TOP_RIGHT: "TOP_RIGHT",
    LEFT: "LEFT",
    RIGHT: "RIGHT",
    BOTTOM_LEFT: "BOTTOM_LEFT", 
    BOTTOM: "BOTTOM",
    BOTTOM_RIGHT: "BOTTOM_RIGHT"
}

// Cursor shape mappings
var _CursorShapes = {
    TOP_LEFT: Qt.SizeFDiagCursor,
    TOP: Qt.SizeVerCursor,
    TOP_RIGHT: Qt.SizeBDiagCursor,
    LEFT: Qt.SizeHorCursor,
    RIGHT: Qt.SizeHorCursor,
    BOTTOM_LEFT: Qt.SizeBDiagCursor,
    BOTTOM: Qt.SizeVerCursor,
    BOTTOM_RIGHT: Qt.SizeFDiagCursor
};

// Handle direction lookup table
var _HandleDirections = {
    TOP_LEFT: { x: -1, y: -1 },
    TOP: { x: 0, y: -1 },
    TOP_RIGHT: { x: 1, y: -1 },
    LEFT: { x: -1, y: 0 },
    RIGHT: { x: 1, y: 0 },
    BOTTOM_LEFT: { x: -1, y: 1 },
    BOTTOM: { x: 0, y: 1 },
    BOTTOM_RIGHT: { x: 1, y: 1 }
}

function Frame(x, y, width, height) {
    this.x = x || 0
    this.y = y || 0
    this.width = width || 0
    this.height = height || 0
    
    this.toQtRect = function() {
        return Qt.rect(this.x, this.y, this.width, this.height)
    }
}

function Point(x, y) {
    this.x = x || 0
    this.y = y || 0
}

// Keep rotation in 0-360 range
function normalizeRotation(rotationDegrees) {
    return ((rotationDegrees % 360) + 360) % 360
}

// Map logical handle type to visual handle type based on rotation angle
// so we can show the correct cursor pointing in (roughly) the right direction
function getVisualHandleType(logicalHandleType, rotationDegrees) {
    var normalizedRotation = normalizeRotation(rotationDegrees)
    
    // Round to nearest 90 degrees for handle mapping
    var quadrant = Math.round(normalizedRotation / 90) % 4
    
    var result
    if (quadrant === 0) {
        // 0 degrees - no change
        result = logicalHandleType
    } else if (quadrant === 1) {
        // 90 degrees
        switch(logicalHandleType) {
            case HandleTypes.TOP_LEFT: result = HandleTypes.TOP_RIGHT; break
            case HandleTypes.TOP: result = HandleTypes.RIGHT; break
            case HandleTypes.TOP_RIGHT: result = HandleTypes.BOTTOM_RIGHT; break
            case HandleTypes.LEFT: result = HandleTypes.TOP; break
            case HandleTypes.RIGHT: result = HandleTypes.BOTTOM; break
            case HandleTypes.BOTTOM_LEFT: result = HandleTypes.TOP_LEFT; break
            case HandleTypes.BOTTOM: result = HandleTypes.LEFT; break
            case HandleTypes.BOTTOM_RIGHT: result = HandleTypes.BOTTOM_LEFT; break
            default: result = logicalHandleType; break
        }
    } else if (quadrant === 2) {
        // 180 degrees
        switch(logicalHandleType) {
            case HandleTypes.TOP_LEFT: result = HandleTypes.BOTTOM_RIGHT; break
            case HandleTypes.TOP: result = HandleTypes.BOTTOM; break
            case HandleTypes.TOP_RIGHT: result = HandleTypes.BOTTOM_LEFT; break
            case HandleTypes.LEFT: result = HandleTypes.RIGHT; break
            case HandleTypes.RIGHT: result = HandleTypes.LEFT; break
            case HandleTypes.BOTTOM_LEFT: result = HandleTypes.TOP_RIGHT; break
            case HandleTypes.BOTTOM: result = HandleTypes.TOP; break
            case HandleTypes.BOTTOM_RIGHT: result = HandleTypes.TOP_LEFT; break
            default: result = logicalHandleType; break
        }
    } else {
        // 270 degrees
        switch(logicalHandleType) {
            case HandleTypes.TOP_LEFT: result = HandleTypes.BOTTOM_LEFT; break
            case HandleTypes.TOP: result = HandleTypes.LEFT; break
            case HandleTypes.TOP_RIGHT: result = HandleTypes.TOP_LEFT; break
            case HandleTypes.LEFT: result = HandleTypes.BOTTOM; break
            case HandleTypes.RIGHT: result = HandleTypes.TOP; break
            case HandleTypes.BOTTOM_LEFT: result = HandleTypes.BOTTOM_RIGHT; break
            case HandleTypes.BOTTOM: result = HandleTypes.RIGHT; break
            case HandleTypes.BOTTOM_RIGHT: result = HandleTypes.TOP_RIGHT; break
            default: result = logicalHandleType; break
        }
    }
    
    return result
}

function getCursorShape(handleType) {
    return _CursorShapes[handleType] || Qt.ArrowCursor
}

// Calculate new frame dimensions based on handle movement, the original frame and the rotation angle
function calculateResize(handleType, scaledDeltaX, scaledDeltaY, frameSize, lockRatio, modifiers, rotationAngle) {
    var resizedFrame = _calculateResize(handleType, scaledDeltaX, scaledDeltaY, frameSize, lockRatio, modifiers)

    // If rotation align it
    if (rotationAngle && Math.abs(rotationAngle) >= 0.1) {
        return _alignResizedFrameToAnchor(handleType, rotationAngle, resizedFrame, frameSize).toQtRect()
    }

    return resizedFrame.toQtRect()
}

function _isCornerHandle(handleType) {
    return handleType === HandleTypes.TOP_LEFT || 
           handleType === HandleTypes.TOP_RIGHT || 
           handleType === HandleTypes.BOTTOM_LEFT || 
           handleType === HandleTypes.BOTTOM_RIGHT
}

function _affectsX(handleType) {
    return handleType === HandleTypes.LEFT || 
           handleType === HandleTypes.RIGHT ||
           _isCornerHandle(handleType)
}

function _affectsY(handleType) {
    return handleType === HandleTypes.TOP || 
           handleType === HandleTypes.BOTTOM ||
           _isCornerHandle(handleType)
}

// Get direction multipliers for a handle type
function _getDirection(handleType) {
    return _HandleDirections[handleType] || { x: 0, y: 0 }
}

// Get X direction multiplier (-1 for shrink left, +1 for expand right, 0 for no effect)
function _getXDirection(handleType) {
    return _getDirection(handleType).x
}

// Get Y direction multiplier (-1 for shrink up, +1 for expand down, 0 for no effect)
function _getYDirection(handleType) {
    return _getDirection(handleType).y
}

function _calculateResize(handleType, scaledDeltaX, scaledDeltaY, frameSize, lockRatio, modifiers) {
    var adjustedFrame = new Frame(frameSize.x, frameSize.y, frameSize.width, frameSize.height)
    
    var xDirection = _getXDirection(handleType)
    var yDirection = _getYDirection(handleType)
    
    var effectiveX = scaledDeltaX
    var effectiveY = scaledDeltaY
    
    // Aspect ratio constraints
    if (lockRatio > 0 || modifiers & Qt.ShiftModifier) {
        var aspectRatio = lockRatio > 0 ? lockRatio :frameSize.width / frameSize.height
        
        if (_isCornerHandle(handleType)) {
            // Corner handles affect both X and Y. We pick X as primary and calculate Y based on aspect ratio
            var sameSignDirections = (xDirection * yDirection) > 0
            
            if (sameSignDirections) {
                // TOP_LEFT, BOTTOM_RIGHT
                effectiveY = effectiveX / aspectRatio
            } else {
                // TOP_RIGHT, BOTTOM_LEFT
                effectiveY = -effectiveX / aspectRatio
            }
        } else if (_affectsX(handleType)) {
            // X-only handle: calculate Y based on aspect ratio
            effectiveY = effectiveX / aspectRatio
        } else if (_affectsY(handleType)) {
            // Y-only handle: calculate X based on aspect ratio  
            effectiveX = effectiveY * aspectRatio
        }
    }
    
    // Apply size changes based on handle type and movement direction
    if (_affectsX(handleType)) {
        if (xDirection < 0) {
            // Left edge handles: move left edge
            // Rightward movement shrinks from left, Leftward movement expands to left
            adjustedFrame.width = Math.max(1, frameSize.width - effectiveX)
            adjustedFrame.x = frameSize.x + effectiveX
        } else {
            // Right edge handles: move right edge
            // Rightward movement expands to right, Leftward movement shrinks from right
            adjustedFrame.width = Math.max(1, frameSize.width + effectiveX)
        }
    }
    
    if (_affectsY(handleType)) {
        if (yDirection < 0) {
            // Top edge handles: move top edge
            // Downward movement shrinks from top, Upward movement expands to top
            adjustedFrame.height = Math.max(1, frameSize.height - effectiveY)
            adjustedFrame.y = frameSize.y + effectiveY
        } else {
            // Bottom edge handles: move bottom edge
            // Downward movement expands to bottom, Upward movement shrinks from bottom
            adjustedFrame.height = Math.max(1, frameSize.height + effectiveY)
        }
    }
    
    // center-based scaling (Ctrl modifier)
    if (modifiers & Qt.ControlModifier) {
        if (_affectsX(handleType)) {
            // Right handles: rightward movement expands both sides
            // Left handles: rightward movement shrinks both sides
            var xDelta = effectiveX * xDirection
            adjustedFrame.width = Math.max(1, frameSize.width + 2 * xDelta)
            adjustedFrame.x = frameSize.x - xDelta
        }
        
        if (_affectsY(handleType)) {
            // Bottom handles: downward movement expands both sides
            // Top handles: downward movement shrinks both sides
            var yDelta = effectiveY * yDirection
            adjustedFrame.height = Math.max(1, frameSize.height + 2 * yDelta)
            adjustedFrame.y = frameSize.y - yDelta
        }
    }
    
    // For aspect ratio with edge handles, center the unchanged dimension
    if ((lockRatio > 0 || modifiers & Qt.ShiftModifier) && !_isCornerHandle(handleType)) {
        if (_affectsX(handleType) && Math.abs(effectiveY) > 0) {
            // X handle changed Y: center vertically
            adjustedFrame.y = frameSize.y + (frameSize.height - adjustedFrame.height) / 2
        } else if (_affectsY(handleType) && Math.abs(effectiveX) > 0) {
            // Y handle changed X: center horizontally  
            adjustedFrame.x = frameSize.x + (frameSize.width - adjustedFrame.width) / 2
        }
    }
    
    return adjustedFrame
}

// Rotate point around center
function _rotatePoint(x, y, centerX, centerY, angleDegrees) {
    var angleRad = angleDegrees * Math.PI / 180
    var cos = Math.cos(angleRad)
    var sin = Math.sin(angleRad)
    
    var dx = x - centerX
    var dy = y - centerY
    
    return new Point(centerX + dx * cos - dy * sin, centerY + dx * sin + dy * cos)
}

// Get the anchor point for a handle (the point that should stay fixed during resize)
function _getAnchorPoint(handleType, frame) {
    switch(handleType) {
        case HandleTypes.TOP_LEFT:
            return new Point(frame.x + frame.width, frame.y + frame.height)  // bottom-right
        case HandleTypes.TOP:
            return new Point(frame.x + frame.width / 2, frame.y + frame.height)  // bottom-center
        case HandleTypes.TOP_RIGHT:
            return new Point(frame.x, frame.y + frame.height)  // bottom-left
        case HandleTypes.LEFT:
            return new Point(frame.x + frame.width, frame.y + frame.height / 2)  // right-center
        case HandleTypes.RIGHT:
            return new Point(frame.x, frame.y + frame.height / 2)  // left-center
        case HandleTypes.BOTTOM_LEFT:
            return new Point(frame.x + frame.width, frame.y)  // top-right
        case HandleTypes.BOTTOM:
            return new Point(frame.x + frame.width / 2, frame.y)  // top-center
        case HandleTypes.BOTTOM_RIGHT:
            return new Point(frame.x, frame.y)  // top-left
        default:
            return new Point(frame.x, frame.y)
    }
}

// Rotation-aware resize calculation
function _alignResizedFrameToAnchor(handleType, rotationAngle, resizedFrame, frameSize) {    
    var originalAnchor = _getAnchorPoint(handleType, frameSize)
    var resizedAnchor = _getAnchorPoint(handleType, resizedFrame)
    
    // Calculate the center points for rotation
    var originalCenter = new Point(frameSize.x + frameSize.width / 2, frameSize.y + frameSize.height / 2)
    var resizedCenter = new Point(resizedFrame.x + resizedFrame.width / 2, resizedFrame.y + resizedFrame.height / 2)
    
    // Calculate where the anchor points would be after rotation
    var originalAnchorRotated = _rotatePoint(originalAnchor.x, originalAnchor.y, originalCenter.x, originalCenter.y, rotationAngle)
    var resizedAnchorRotated = _rotatePoint(resizedAnchor.x, resizedAnchor.y, resizedCenter.x, resizedCenter.y, rotationAngle)
    
    // Calculate the offset needed to keep the rotated anchor point fixed
    var offsetX = originalAnchorRotated.x - resizedAnchorRotated.x
    var offsetY = originalAnchorRotated.y - resizedAnchorRotated.y
    
    // Apply the offset to keep the visual anchor point fixed
    var finalFrame = new Frame(resizedFrame.x + offsetX, resizedFrame.y + offsetY, resizedFrame.width, resizedFrame.height)
    
    return finalFrame
}
