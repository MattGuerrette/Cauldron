// AMD AMDUtils code
// 
// Copyright(c) 2018 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stdafx.h"
#include "GltfCommon.h"
#include "GltfHelpers.h"
#include "Misc/Misc.h"

bool GLTFCommon::Load(const std::string &path, const std::string &filename)
{
    Profile p("GLTFCommon::Load");

    m_path = path;

    std::ifstream f(path + filename);
    if (!f)
    {
        Trace(format("The file %s cannot be found\n", filename.c_str()));
        return false;
    }

    f >> j3;

    // Load Buffers
    //
    if (j3.find("buffers") != j3.end())
    {
        const json &buffers = j3["buffers"];
        m_buffersData.resize(buffers.size());
        for (int i = 0; i < buffers.size(); i++)
        {
            const std::string &name = buffers[i]["uri"];
            std::ifstream ff(path + name, std::ios::in | std::ios::binary);

            ff.seekg(0, ff.end);
            std::streamoff length = ff.tellg();
            ff.seekg(0, ff.beg);

            char *p = new char[length];
            ff.read(p, length);
            m_buffersData[i] = p;
        }
    }

    // Load Meshes
    //
    m_pAccessors = &j3["accessors"];
    m_pBufferViews = &j3["bufferViews"];
    const json &meshes = j3["meshes"];
    m_meshes.resize(meshes.size());
    for (int i = 0; i < meshes.size(); i++)
    {
        tfMesh *tfmesh = &m_meshes[i];
        auto &primitives = meshes[i]["primitives"];
        tfmesh->m_pPrimitives.resize(primitives.size());
        for (int p = 0; p < primitives.size(); p++)
        {
            tfPrimitives *pPrimitive = &tfmesh->m_pPrimitives[p];

            int positionId = primitives[p]["attributes"]["POSITION"];
            const json &accessor = m_pAccessors->at(positionId);

            XMVECTOR max = GetVector(GetElementJsonArray(accessor, "max", { 0.0, 0.0, 0.0, 0.0 }));
            XMVECTOR min = GetVector(GetElementJsonArray(accessor, "min", { 0.0, 0.0, 0.0, 0.0 }));

            pPrimitive->m_center = (min + max)*.5;
            pPrimitive->m_radius = max - pPrimitive->m_center;

            pPrimitive->m_center = XMVectorSetW(pPrimitive->m_center, 1.0f); //set the W to 1 since this is a position not a direction
        }
    }

    // Load lights
    //
    if (j3.find("extensions") != j3.end())
    {
        const json &extensions = j3["extensions"];
        if (extensions.find("KHR_lights_punctual") != extensions.end())
        {
            const json &KHR_lights_punctual = extensions["KHR_lights_punctual"];
            if (KHR_lights_punctual.find("lights") != KHR_lights_punctual.end())
            {
                const json &lights = KHR_lights_punctual["lights"];
                m_lights.resize(lights.size());
                for (int i = 0; i < lights.size(); i++)
                {
                    json::object_t light = lights[i];
                    m_lights[i].m_color = GetElementVector(light, "color", XMVectorSet(1, 1, 1, 0));
                    m_lights[i].m_range = GetElementFloat(light, "range", 105);
                    m_lights[i].m_intensity = GetElementFloat(light, "intensity", 1);
                    m_lights[i].m_innerConeAngle = GetElementFloat(light, "spot/innerConeAngle", 0);
                    m_lights[i].m_outerConeAngle = GetElementFloat(light, "spot/outerConeAngle", XM_PIDIV4);

                    std::string name = GetElementString(light, "type", "");
                    if (name == "spot")
                        m_lights[i].m_type = tfLight::LIGHT_SPOTLIGHT;
                    else if (name == "point")
                        m_lights[i].m_type = tfLight::LIGHT_POINTLIGHT;
                    else if (name == "directional")
                        m_lights[i].m_type = tfLight::LIGHT_DIRECTIONAL;
                }
            }
        }
    }

    // Load cameras
    //
    if (j3.find("cameras") != j3.end())
    {
        const json &cameras = j3["cameras"];
        m_cameras.resize(cameras.size());
        for (int i = 0; i < cameras.size(); i++)
        {
            const json &camera = cameras[i];
            tfCamera *tfcamera = &m_cameras[i];

            tfcamera->yfov = GetElementFloat(camera, "perspective/yfov", 0.1f);
            tfcamera->znear = GetElementFloat(camera, "perspective/znear", 0.1f);
            tfcamera->zfar = GetElementFloat(camera, "perspective/zfar", 100.0f);
            tfcamera->m_nodeIndex = -1;
        }
    }

    // Load nodes
    //
    if (j3.find("nodes") != j3.end())
    {
        const json &nodes = j3["nodes"];
        m_nodes.resize(nodes.size());
        for (int i = 0; i < nodes.size(); i++)
        {
            tfNode *tfnode = &m_nodes[i];

            // Read node data
            //
            json::object_t node = nodes[i];

            if (node.find("children") != node.end())
            {
                for (int c = 0; c < node["children"].size(); c++)
                {
                    int nodeID = node["children"][c];
                    tfnode->m_children.push_back(nodeID);
                }
            }

            tfnode->meshIndex = GetElementInt(node, "mesh", -1);
            tfnode->skinIndex = GetElementInt(node, "skin", -1);

            int cameraIdx = GetElementInt(node, "camera", -1);
            if (cameraIdx >= 0)
                m_cameras[cameraIdx].m_nodeIndex = i;

            int lightIdx = GetElementInt(node, "extensions/KHR_lights_punctual/light", -1);
            if (lightIdx >= 0)
                m_lights[lightIdx].m_nodeIndex = i;

            tfnode->m_tranform.m_translation = GetElementVector(node, "translation", XMVectorSet(0, 0, 0, 0));
            tfnode->m_tranform.m_scale = GetElementVector(node, "scale", XMVectorSet(1, 1, 1, 0));

            if (node.find("name") != node.end())
                tfnode->m_name = GetElementString(node, "name", "unnamed");

            if (node.find("rotation") != node.end())
                tfnode->m_tranform.m_rotation = XMMatrixRotationQuaternion(GetVector(node["rotation"].get<json::array_t>()));
            else if (node.find("matrix") != node.end())
                tfnode->m_tranform.m_rotation = GetMatrix(node["matrix"].get<json::array_t>());
            else
                tfnode->m_tranform.m_rotation = XMMatrixIdentity();
        }
    }

    // Load scenes
    //
    if (j3.find("scenes") != j3.end())
    {
        const json &scenes = j3["scenes"];
        m_scenes.resize(scenes.size());
        for (int i = 0; i < scenes.size(); i++)
        {
            auto scene = scenes[i];
            for (int n = 0; n < scene["nodes"].size(); n++)
            {
                int nodeId = scene["nodes"][n];
                m_scenes[i].m_nodes.push_back(nodeId);
            }
        }
    }

    // Load skins
    //
    if (j3.find("skins") != j3.end())
    {
        const json &skins = j3["skins"];
        m_skins.resize(skins.size());
        for (uint32_t i = 0; i < skins.size(); i++)
        {
            GetBufferDetails(skins[i]["inverseBindMatrices"].get<int>(), &m_skins[i].m_InverseBindMatrices);

            if (skins[i].find("skeleton") != skins[i].end())
                m_skins[i].m_pSkeleton = &m_nodes[skins[i]["skeleton"]];

            const json &joints = skins[i]["joints"];
            for (uint32_t n = 0; n < joints.size(); n++)
            {
                m_skins[i].m_jointsNodeIdx.push_back(joints[n]);
            }

        }
    }

    // Load animations
    //
    if (j3.find("animations") != j3.end())
    {
        const json &animations = j3["animations"];
        m_animations.resize(animations.size());
        for (int i = 0; i < animations.size(); i++)
        {
            const json &channels = animations[i]["channels"];
            const json &samplers = animations[i]["samplers"];

            tfAnimation *tfanim = &m_animations[i];
            for (int c = 0; c < channels.size(); c++)
            {
                json::object_t channel = channels[c];
                int sampler = channel["sampler"];
                int node = GetElementInt(channel, "target/node", -1);
                std::string path = GetElementString(channel, "target/path", std::string());

                tfChannel *tfchannel;

                auto ch = tfanim->m_channels.find(node);
                if (ch == tfanim->m_channels.end())
                {
                    tfchannel = &tfanim->m_channels[node];
                }
                else
                {
                    tfchannel = &ch->second;
                }

                tfSampler *tfsmp = new tfSampler();

                // Get time line
                //
                GetBufferDetails(samplers[sampler]["input"], &tfsmp->m_time);
                assert(tfsmp->m_time.m_stride == 4);

                tfanim->m_duration = std::max<float>(tfanim->m_duration, *(float*)tfsmp->m_time.Get(tfsmp->m_time.m_count - 1));

                // Get value line
                //
                GetBufferDetails(samplers[sampler]["output"], &tfsmp->m_value);

                // Index appropriately
                // 
                if (path == "translation")
                {
                    tfchannel->m_pTranslation = tfsmp;
                    assert(tfsmp->m_value.m_stride == 3 * 4);
                    assert(tfsmp->m_value.m_dimension == 3);
                }
                else if (path == "rotation")
                {
                    tfchannel->m_pRotation = tfsmp;
                    assert(tfsmp->m_value.m_stride == 4 * 4);
                    assert(tfsmp->m_value.m_dimension == 4);
                }
                else if (path == "scale")
                {
                    tfchannel->m_pScale = tfsmp;
                    assert(tfsmp->m_value.m_stride == 3 * 4);
                    assert(tfsmp->m_value.m_dimension == 3);
                }
            }
        }
    }

    InitTransformedData();

    return true;
}

void GLTFCommon::Unload()
{
    for (int i = 0; i < m_buffersData.size(); i++)
    {
        delete (m_buffersData[i]);
    }
    m_buffersData.clear();

    m_animations.clear();
    m_nodes.clear();
    m_scenes.clear();

    j3.clear();
}

//
// Animates the matrices (they are still in object space)
//
void GLTFCommon::SetAnimationTime(uint32_t animationIndex, float time)
{
    if (animationIndex < m_animations.size())
    {
        tfAnimation *anim = &m_animations[animationIndex];

        //loop animation
        time = fmod(time, anim->m_duration);

        for (auto it = anim->m_channels.begin(); it != anim->m_channels.end(); it++)
        {
            Transform *pSourceTrans = &m_nodes[it->first].m_tranform;
            Transform animated;

            float frac, *pCurr, *pNext;

            // Animate translation
            //
            if (it->second.m_pTranslation != NULL)
            {
                it->second.m_pTranslation->SampleLinear(time, &frac, &pCurr, &pNext);
                animated.m_translation = (1.0f - frac) * XMVectorSet(pCurr[0], pCurr[1], pCurr[2], 0) + (frac)*XMVectorSet(pNext[0], pNext[1], pNext[2], 0);
            }
            else
            {
                animated.m_translation = pSourceTrans->m_translation;
            }

            // Animate rotation
            //
            if (it->second.m_pRotation != NULL)
            {
                it->second.m_pRotation->SampleLinear(time, &frac, &pCurr, &pNext);
                animated.m_rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(XMVectorSet(pCurr[0], pCurr[1], pCurr[2], pCurr[3]), XMVectorSet(pNext[0], pNext[1], pNext[2], pNext[3]), frac));
            }
            else
            {
                animated.m_rotation = pSourceTrans->m_rotation;
            }

            // Animate scale
            //
            if (it->second.m_pScale != NULL)
            {
                it->second.m_pScale->SampleLinear(time, &frac, &pCurr, &pNext);
                animated.m_scale = (1.0f - frac) * XMVectorSet(pCurr[0], pCurr[1], pCurr[2], 0) + (frac)*XMVectorSet(pNext[0], pNext[1], pNext[2], 0);
            }
            else
            {
                animated.m_scale = pSourceTrans->m_scale;
            }

            m_animatedMats[it->first] = animated.GetWorldMat();
        }
    }
}

void GLTFCommon::GetBufferDetails(int accessor, tfAccessor *pAccessor)
{
    const json &inAccessor = m_pAccessors->at(accessor);

    int32_t bufferViewIdx = inAccessor.value("bufferView", -1);
    assert(bufferViewIdx >= 0);
    const json &bufferView = m_pBufferViews->at(bufferViewIdx);

    int32_t bufferIdx = bufferView.value("buffer", -1);
    assert(bufferIdx >= 0);

    char *buffer = m_buffersData[bufferIdx];

    int32_t offset = bufferView.value("byteOffset", 0);

    int byteLength = bufferView["byteLength"];

    int32_t byteOffset = inAccessor.value("byteOffset", 0);
    offset += byteOffset;
    byteLength -= byteOffset;

    pAccessor->m_data = &buffer[offset];
    pAccessor->m_dimension = GetDimensions(inAccessor["type"]);
    pAccessor->m_type = GetFormatSize(inAccessor["componentType"]);
    pAccessor->m_stride = pAccessor->m_dimension * pAccessor->m_type;
    pAccessor->m_count = inAccessor["count"];
}

void GLTFCommon::GetAttributesAccessors(const json &gltfAttributes, std::vector<char*> *pStreamNames, std::vector<tfAccessor> *pAccessors)
{
    int streamIndex = 0;
    for (int s = 0; s < pStreamNames->size(); s++)
    {
        auto attr = gltfAttributes.find(pStreamNames->at(s));
        if (attr != gltfAttributes.end())
        {
            tfAccessor accessor;
            GetBufferDetails(attr.value(), &accessor);
            pAccessors->push_back(accessor);
        }
    }
}

//
// Given a mesh find the skin it belongs to
//
int GLTFCommon::FindMeshSkinId(int meshId)
{
    for (int i = 0; i < m_nodes.size(); i++)
    {
        if (m_nodes[i].meshIndex == meshId)
        {
            return m_nodes[i].skinIndex;
        }
    }

    return -1;
}

//
// given a skinId return the size of the skeleton matrices (vulkan needs this to compute the offsets into the uniform buffers)
//
int GLTFCommon::GetInverseBindMatricesBufferSizeByID(int id)
{
    if (id == -1 || (id >= m_skins.size()))
        return -1;

    return m_skins[id].m_InverseBindMatrices.m_count * (4 * 4 * sizeof(float));
}

//
// Transforms a node hierarchy recursively 
//
void GLTFCommon::TransformNodes(tfNode *pRootNode, XMMATRIX world, std::vector<tfNodeIdx> *pNodes, GLTFCommonTransformed *pTransformed)
{
    pTransformed->m_worldSpaceMats.resize(m_nodes.size());

    for (uint32_t n = 0; n < pNodes->size(); n++)
    {
        uint32_t nodeIdx = pNodes->at(n);

        XMMATRIX m = m_animatedMats[nodeIdx] * world;
        pTransformed->m_worldSpaceMats[nodeIdx] = m;

        TransformNodes(pRootNode, m, &m_nodes[nodeIdx].m_children, pTransformed);
    }
}

//
// Initializes the GLTFCommonTransformed structure 
//
void GLTFCommon::InitTransformedData()
{
    // we need to init 2 frames, the current and the previous one, this is needed for the MVs
    for (int frames = 0; frames < 2; frames++)
    {
        // initializes matrix buffers to have the same dimension as the nodes
        m_transformedData[frames].m_worldSpaceMats.resize(m_nodes.size());

        // same thing for the skinning matrices but using the size of the InverseBindMatrices
        for (uint32_t i = 0; i < m_skins.size(); i++)
        {
            m_transformedData[frames].m_worldSpaceSkeletonMats[i].resize(m_skins[i].m_InverseBindMatrices.m_count);
        }
    }

    m_pCurrentFrameTransformedData = &m_transformedData[0];
    m_pPreviousFrameTransformedData = &m_transformedData[1];

    // sets the animated data to the default values of the nodes
    // later on these values can be updated by the SetAnimationTime function
    m_animatedMats.resize(m_nodes.size());
    for (uint32_t i = 0; i < m_nodes.size(); i++)
    {
        m_animatedMats[i] = m_nodes[i].m_tranform.GetWorldMat();
    }
}

//
// Takes the animated matrices and processes the hierarchy, also computes the skinning matrix buffers. 
//
void GLTFCommon::TransformScene(int sceneIndex, XMMATRIX world)
{
    //swap transformation buffers, we need to keep the last frame data around so we can compute the motion vectors
    GLTFCommonTransformed *pTmp = m_pCurrentFrameTransformedData;
    m_pCurrentFrameTransformedData = m_pPreviousFrameTransformedData;
    m_pPreviousFrameTransformedData = pTmp;

    // transform all the nodes of the scene
    //           
    std::vector<int> sceneNodes = { m_scenes[sceneIndex].m_nodes };
    TransformNodes(m_nodes.data(), world, &sceneNodes, m_pCurrentFrameTransformedData);

    //process skeletons, takes the skinning matrices from the scene and puts them into a buffer that the vertex shader will consume
    //
    for (uint32_t i = 0; i < m_skins.size(); i++)
    {
        tfSkins &skin = m_skins[i];

        //pick the matrices that affect the skin and multiply by the inverse of the bind         
        XMMATRIX *pM = (XMMATRIX *)skin.m_InverseBindMatrices.m_data;
        std::vector<XMMATRIX> &skinningMats = m_pCurrentFrameTransformedData->m_worldSpaceSkeletonMats[i];
        for (int j = 0; j < skin.m_InverseBindMatrices.m_count; j++)
        {
            skinningMats[j] = XMMatrixMultiply(pM[j], m_pCurrentFrameTransformedData->m_worldSpaceMats[skin.m_jointsNodeIdx[j]]);
        }
    }
}

bool GLTFCommon::GetCamera(uint32_t cameraIdx, Camera *pCam)
{
    if (cameraIdx < 0 || cameraIdx >= m_cameras.size())
    {
        pCam = NULL;
        return false;
    }

    XMMATRIX *pMats = m_pCurrentFrameTransformedData->m_worldSpaceMats.data();

    XMMATRIX cameraMat = pMats[m_cameras[cameraIdx].m_nodeIndex];
    pCam->SetMatrix(cameraMat);
    pCam->SetFov(m_cameras[cameraIdx].yfov, 1280, 720, m_cameras[cameraIdx].znear, m_cameras[cameraIdx].zfar);

    return true;
}

//
// Sets the per frame data from the GLTF, returns a pointer to it in case the user wants to override some values
// The scene needs to be animated and transformed before we can set the per_frame data. We need those final matrices for the lights and the camera.
//
per_frame *GLTFCommon::SetPerFrameData(const Camera &cam)
{
    XMMATRIX *pMats = m_pCurrentFrameTransformedData->m_worldSpaceMats.data();

    //Sets the camera
    m_perFrameData.mCameraViewProj = cam.GetView() * cam.GetProjection();
    m_perFrameData.mInverseCameraViewProj = XMMatrixInverse(nullptr, m_perFrameData.mCameraViewProj);
    m_perFrameData.cameraPos = cam.GetPosition();

    // Process lights
    m_perFrameData.lightCount = (int32_t)m_lights.size();
    for (int i = 0; i < m_lights.size(); i++)
    {
        XMMATRIX lightMat = pMats[m_lights[i].m_nodeIndex];
        XMMATRIX lightView = XMMatrixInverse(nullptr, lightMat);

        Light* pSL = &m_perFrameData.lights[i];
        if (m_lights[i].m_type == LightType_Spot)
            pSL->mLightViewProj = lightView * XMMatrixPerspectiveFovRH(m_lights[i].m_outerConeAngle * 2.0f, 1, .1f, 100.0f);
        else if (m_lights[i].m_type == LightType_Directional)
            pSL->mLightViewProj = lightView * XMMatrixOrthographicRH(30.0, 30.0, 0.1f, 100.0f);

        GetXYZ(pSL->direction, XMVector4Transform(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMMatrixTranspose(lightView)));
        GetXYZ(pSL->color, m_lights[i].m_color);
        pSL->range = m_lights[i].m_range;
        pSL->intensity = m_lights[i].m_intensity;
        GetXYZ(pSL->position, lightMat.r[3]);
        pSL->outerConeCos = cosf(m_lights[i].m_outerConeAngle);
        pSL->innerConeCos = cosf(m_lights[i].m_innerConeAngle);
        pSL->type = m_lights[i].m_type;
        pSL->depthBias = 0.0001f;
    }
    return &m_perFrameData;
}


tfNodeIdx GLTFCommon::AddNode(tfNode node)
{
    m_nodes.push_back(node);
    tfNodeIdx idx = (tfNodeIdx)(m_nodes.size() - 1);
    m_scenes[0].m_nodes.push_back(idx);
    
    m_animatedMats.push_back(node.m_tranform.GetWorldMat());

    return idx;
}

int GLTFCommon::AddLight(tfLight light)
{
    m_lights.push_back(light);    
    return (int)(m_lights.size() - 1);
}
