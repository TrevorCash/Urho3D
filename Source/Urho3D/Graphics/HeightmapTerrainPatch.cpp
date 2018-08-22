//
// Copyright (c) 2008-2018 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Graphics/Camera.h"
#include "../Graphics/DebugRenderer.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/Material.h"
#include "../Graphics/OcclusionBuffer.h"
#include "../Graphics/OctreeQuery.h"
#include "../Graphics/HeightmapTerrain.h"
#include "../Graphics/HeightmapTerrainPatch.h"
#include "../Graphics/VertexBuffer.h"
#include "../IO/Log.h"
#include "../Scene/Node.h"

#include "../DebugNew.h"

namespace Urho3D
{

static const float LOD_CONSTANT = 1.0f / 150.0f;

extern const char* GEOMETRY_CATEGORY;

HeightmapTerrainPatch::HeightmapTerrainPatch(Context* context) :
    Drawable(context, DRAWABLE_GEOMETRY),
    geometry_(new Geometry(context)),
    maxLodGeometry_(new Geometry(context)),
    occlusionGeometry_(new Geometry(context)),
    vertexBuffer_(new VertexBuffer(context)),
    coordinates_(IntVector2::ZERO),
    lodLevel_(0)
{
    geometry_->SetVertexBuffer(0, vertexBuffer_);
    maxLodGeometry_->SetVertexBuffer(0, vertexBuffer_);
    occlusionGeometry_->SetVertexBuffer(0, vertexBuffer_);

    batches_.Resize(1);
    batches_[0].geometry_ = geometry_;
    batches_[0].geometryType_ = GEOM_STATIC_NOINSTANCING;
}

HeightmapTerrainPatch::~HeightmapTerrainPatch() = default;

void HeightmapTerrainPatch::RegisterObject(Context* context)
{
    context->RegisterFactory<HeightmapTerrainPatch>();
}

void HeightmapTerrainPatch::ProcessRayQuery(const RayOctreeQuery& query, PODVector<RayQueryResult>& results)
{
    RayQueryLevel level = query.level_;

    switch (level)
    {
    case RAY_AABB:
        Drawable::ProcessRayQuery(query, results);
        break;

    case RAY_OBB:
    case RAY_TRIANGLE:
        {
            Matrix3x4 inverse(node_->GetWorldTransform().Inverse());
            Ray localRay = query.ray_.Transformed(inverse);
            float distance = localRay.HitDistance(boundingBox_);
            Vector3 normal = -query.ray_.direction_;

            if (level == RAY_TRIANGLE && distance < query.maxDistance_)
            {
                Vector3 geometryNormal;
                distance = geometry_->GetHitDistance(localRay, &geometryNormal);
                normal = (node_->GetWorldTransform() * Vector4(geometryNormal, 0.0f)).Normalized();
            }

            if (distance < query.maxDistance_)
            {
                RayQueryResult result;
                result.position_ = query.ray_.origin_ + distance * query.ray_.direction_;
                result.normal_ = normal;
                result.distance_ = distance;
                result.drawable_ = this;
                result.node_ = node_;
                result.subObject_ = M_MAX_UNSIGNED;
                results.Push(result);
            }
        }
        break;

    case RAY_TRIANGLE_UV:
        URHO3D_LOGWARNING("RAY_TRIANGLE_UV query level is not supported for TerrainPatch component");
        break;
    }
}

void HeightmapTerrainPatch::UpdateBatches(const RenderFrameInfo& frame)
{
    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    distance_ = frame.camera_->GetDistance(GetWorldBoundingBox().Center());

    float scale = worldTransform.Scale().DotProduct(DOT_SCALE);
    lodDistance_ = frame.camera_->GetLodDistance(distance_, scale, lodBias_);

    batches_[0].distance_ = distance_;
    batches_[0].worldTransform_ = &worldTransform;

    unsigned newLodLevel = 0;
    for (unsigned i = 0; i < lodErrors_.Size(); ++i)
    {
        if (lodErrors_[i] / lodDistance_ > LOD_CONSTANT)
            break;
        else
            newLodLevel = i;
    }

    lodLevel_ = GetCorrectedLodLevel(newLodLevel);
}

void HeightmapTerrainPatch::UpdateGeometry(const RenderFrameInfo& frame)
{
    if (vertexBuffer_->IsDataLost())
    {
        if (owner_)
            owner_->CreatePatchGeometry(this);
        else
            vertexBuffer_->ClearDataLost();
    }

    if (owner_)
        owner_->UpdatePatchLod(this);
}

UpdateGeometryType HeightmapTerrainPatch::GetUpdateGeometryType()
{
    // Because there is a latency in starting worker thread updates, and the update of terrain patch LOD should not take
    // much time, always update in the main thread
    return UPDATE_MAIN_THREAD;
}

Geometry* HeightmapTerrainPatch::GetLodGeometry(unsigned batchIndex, unsigned level)
{
    if (!level)
        return maxLodGeometry_;
    else
        return geometry_;
}

unsigned HeightmapTerrainPatch::GetNumOccluderTriangles()
{
    // Check that the material is suitable for occlusion (default material always is)
    Material* mat = batches_[0].material_;
    if (mat && !mat->GetOcclusion())
        return 0;
    else
        return occlusionGeometry_->GetIndexCount() / 3;
}

bool HeightmapTerrainPatch::DrawOcclusion(OcclusionBuffer* buffer)
{
    // Check that the material is suitable for occlusion (default material always is) and set culling mode
    Material* material = batches_[0].material_;
    if (material)
    {
        if (!material->GetOcclusion())
            return true;
        buffer->SetCullMode(material->GetCullMode());
    }
    else
        buffer->SetCullMode(CULL_CCW);

    const unsigned char* vertexData;
    unsigned vertexSize;
    const unsigned char* indexData;
    unsigned indexSize;
    const PODVector<VertexElement>* elements;

    occlusionGeometry_->GetRawData(vertexData, vertexSize, indexData, indexSize, elements);
    // Check for valid geometry data
    if (!vertexData || !indexData || !elements || VertexBuffer::GetElementOffset(*elements, TYPE_VECTOR3, SEM_POSITION) != 0)
        return false;

    // Draw and check for running out of triangles
    return buffer->AddTriangles(node_->GetWorldTransform(), vertexData, vertexSize, indexData, indexSize, occlusionGeometry_->GetIndexStart(),
        occlusionGeometry_->GetIndexCount());
}

void HeightmapTerrainPatch::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (!vertexBuffer_->IsShadowed())
    {
        URHO3D_LOGERROR("TerrainPatch requires shadowed vertex buffer for drawing debug geometry.");
        return;
    }

    if (auto node = GetNode())
    {
        auto geometry = GetLodGeometry(0, lodLevel_);
        auto vbo = geometry->GetVertexBuffer(0);
        auto ibo = geometry->GetIndexBuffer();
        auto vboData = vbo->GetShadowData();
        auto iboData = ibo->GetShadowData();
        debug->AddTriangleMesh(vboData, vbo->GetVertexSize(), geometry->GetVertexStart(), iboData, ibo->GetIndexSize(),
                               geometry->GetIndexStart(), geometry->GetIndexCount(), node->GetWorldTransform(),
                               Color::GREEN, depthTest);
    }
}

void HeightmapTerrainPatch::SetOwner(HeightmapTerrain* terrain)
{
    owner_ = terrain;
}

void HeightmapTerrainPatch::SetNeighbors(HeightmapTerrainPatch* north, HeightmapTerrainPatch* south, HeightmapTerrainPatch* west, HeightmapTerrainPatch* east)
{
    north_ = north;
    south_ = south;
    west_ = west;
    east_ = east;
}

void HeightmapTerrainPatch::SetMaterial(Material* material)
{
    batches_[0].material_ = material;
}

void HeightmapTerrainPatch::SetBoundingBox(const BoundingBox& box)
{
    boundingBox_ = box;
    OnMarkedDirty(node_);
}

void HeightmapTerrainPatch::SetCoordinates(const IntVector2& coordinates)
{
    coordinates_ = coordinates;
}

void HeightmapTerrainPatch::ResetLod()
{
    lodLevel_ = 0;
}

Geometry* HeightmapTerrainPatch::GetGeometry() const
{
    return geometry_;
}

Geometry* HeightmapTerrainPatch::GetMaxLodGeometry() const
{
    return maxLodGeometry_;
}

Geometry* HeightmapTerrainPatch::GetOcclusionGeometry() const
{
    return occlusionGeometry_;
}

VertexBuffer* HeightmapTerrainPatch::GetVertexBuffer() const
{
    return vertexBuffer_;
}

HeightmapTerrain* HeightmapTerrainPatch::GetOwner() const
{
    return owner_;
}

void HeightmapTerrainPatch::OnWorldBoundingBoxUpdate()
{
    worldBoundingBox_ = boundingBox_.Transformed(node_->GetWorldTransform());
}

unsigned HeightmapTerrainPatch::GetCorrectedLodLevel(unsigned lodLevel)
{
    if (north_)
        lodLevel = Min(lodLevel, north_->GetLodLevel() + 1);
    if (south_)
        lodLevel = Min(lodLevel, south_->GetLodLevel() + 1);
    if (west_)
        lodLevel = Min(lodLevel, west_->GetLodLevel() + 1);
    if (east_)
        lodLevel = Min(lodLevel, east_->GetLodLevel() + 1);

    return lodLevel;
}

}