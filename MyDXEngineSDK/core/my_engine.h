#pragma once

#pragma comment( lib, "xinput.lib")
#pragma comment(lib, "dxcompiler.lib")


#include "root_signature.h"
#include "render_context.h"
#include "vertex_buffer.h"
#include "shader.h"
#include "index_buffer.h"
#include "pipeline_state.h"
#include "rw_structured_buffer.h"
#include "structured_buffer.h"


const int MAX_RENDERING_TARGET = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
using namespace Microsoft::WRL;


const UINT FRAME_BUFFER_W = 1280;
const UINT FRAME_BUFFER_H = 720;
static const int MAX_BONE = 512;