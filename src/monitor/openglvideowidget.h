/*
 * Copyright (c) 2023 Meltytech, LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "videowidget.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

class OpenGLVideoWidget : public VideoWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OpenGLVideoWidget(int id, QObject *parent = nullptr);
    virtual ~OpenGLVideoWidget();

public Q_SLOTS:
    virtual void initialize() override;
    virtual void renderVideo() override;
    virtual void onFrameDisplayed(const SharedFrame &frame) override;

private:
    void createShader();

    QOffscreenSurface m_offscreenSurface;
    std::unique_ptr<QOpenGLShaderProgram> m_shader;
    GLint m_projectionLocation;
    GLint m_modelViewLocation;
    GLint m_vertexLocation;
    GLint m_texCoordLocation;
    GLint m_colorspaceLocation;
    GLint m_textureLocation[3];
    QOpenGLContext *m_quickContext;
    std::unique_ptr<QOpenGLContext> m_context;
    GLuint m_renderTexture[3];
    GLuint m_displayTexture[3];
    bool m_isThreadedOpenGL;
};
