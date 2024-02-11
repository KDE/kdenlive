/*
    SPDX-FileCopyrightText: 2023 Meltytech, LLC
    SPDX-License-Identifier: GPL-3.0-or-later
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
    const QStringList getGPUInfo() override;

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
    QOpenGLFramebufferObject *m_fbo{nullptr};
    GLuint m_renderTexture[3];
    GLuint m_displayTexture[3];
    bool m_isThreadedOpenGL;
};
