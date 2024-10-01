/*
    SPDX-FileCopyrightText: 2023 Meltytech, LLC
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "videowidget.h"

#include <d3d11.h>
#include <directxmath.h>

class D3DVideoWidget : public VideoWidget
{
    Q_OBJECT
public:
    explicit D3DVideoWidget(int id, QObject *parent = nullptr);
    virtual ~D3DVideoWidget();

public Q_SLOTS:
    void initialize() override;
    void beforeRendering() override;
    void renderVideo() override;

private:
    enum Stage { VertexStage, FragmentStage };
    void prepareShader(Stage stage);
    QByteArray compileShader(Stage stage, const QByteArray &source, const QByteArray &entryPoint);
    ID3D11ShaderResourceView *initTexture(const void *p, int width, int height);

    ID3D11Device *m_device = nullptr;
    ID3D11DeviceContext *m_context = nullptr;
    QByteArray m_vert;
    QByteArray m_vertEntryPoint;
    QByteArray m_frag;
    QByteArray m_fragEntryPoint;

    bool m_initialized = false;
    ID3D11Buffer *m_vbuf = nullptr;
    ID3D11Buffer *m_cbuf = nullptr;
    ID3D11VertexShader *m_vs = nullptr;
    ID3D11PixelShader *m_ps = nullptr;
    ID3D11InputLayout *m_inputLayout = nullptr;
    ID3D11RasterizerState *m_rastState = nullptr;
    ID3D11DepthStencilState *m_dsState = nullptr;
    ID3D11ShaderResourceView *m_texture[3] = {nullptr, nullptr, nullptr};

    struct ConstantBuffer
    {
        int32_t colorspace;
    };

    ConstantBuffer m_constants;
};
