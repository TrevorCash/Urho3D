#pragma once
#include "../Resource/Resource.h"
#include "../Core/Context.h"
#include "../Core/Object.h"
#include "Newton.h"
#include "../Math/Interpolations.h"
#include "../Resource/XMLFile.h"

namespace Urho3D {

    ///physics material resource providing data for a physics surface.
    class URHO3D_API PhysicsMaterial : public Resource
    {
        URHO3D_OBJECT(PhysicsMaterial, Resource);

    public:
        friend class PhysicsMaterialContactPair;
        PhysicsMaterial(Context* context) : Resource(context)
        {

        }
        virtual ~PhysicsMaterial() {}

        static void RegisterObject(Context* context) {
            context->RegisterFactory<PhysicsMaterial>();
        }



        virtual bool BeginLoad(Deserializer& source) override
        {
            loadXMLFile_ = new XMLFile(context_);
            return loadXMLFile_->Load(source);
        }


        virtual bool EndLoad() override
        {

            XMLElement rootElem = loadXMLFile_->GetRoot();
            XMLElement slipperiness = rootElem.GetChild("slipperiness");
            slipperiness_ = slipperiness.GetFloat("value");

            return true;
        }


        virtual bool Save(Serializer& dest) const override
        {
            SharedPtr<XMLFile> xml(new XMLFile(context_));
            XMLElement materialElem = xml->CreateRoot("physicsmaterial");
            //Save(materialElem);
            return xml->Save(dest);
        }
        int newtonGroupId = -1;

    protected:

        //friction, restitution, etc..
        float softness_ = 0.2f;//
        float elasticity_ = 0.5f;//how "rubbery the surface along the normal
        float slipperiness_ = 0.5f;//[0 - 1] where 1 is icy and 0 is glue.

        float staticRugosity_  = 1.05f; //roughness used for approximating coefficients of friction. Rugosity = (Surface Area Actual)/(Surface Area Geometrical) [ 1 - 1.1 ]
        float kineticRugosity_ = 1.0f;

        float normalAcceleration_;//acceleration to apply along the normal of the surface. 
        Vector2 tangentalAcceleration_; //acceleration to apply along the surface (x,y)

        /// XML file used while loading.
        SharedPtr<XMLFile> loadXMLFile_;
    };
















    class URHO3D_API PhysicsMaterialContactPair : public Object
    {
        URHO3D_OBJECT(PhysicsMaterialContactPair, Object);
    public:
        PhysicsMaterialContactPair(Context* context) : Object(context)
        {

        }
        virtual ~PhysicsMaterialContactPair() {}

        static void RegisterObject(Context* context) {
            context->RegisterFactory< PhysicsMaterialContactPair>();
        }

        ///computes the metrics of this material contact pair for the given two material definitions.
        void SetMaterials(PhysicsMaterial* material1, PhysicsMaterial* material2)
        {
            float reverseSlipperyness1 = 1 - material1->slipperiness_;
            float reverseSlipperyness2 = 1 - material2->slipperiness_;

            float maxStaticCoeff = 2.0f;

            const float maxRugosity = 1.1f;
            staticFrictionCoef_ = BilinearInterpolation(Vector3(1, maxRugosity, reverseSlipperyness2),
                Vector3(maxRugosity, maxRugosity, maxStaticCoeff),
                Vector3(1, 1, Min(reverseSlipperyness1, reverseSlipperyness2)),
                Vector3(maxRugosity, 1, reverseSlipperyness1),
                Vector2(material1->staticRugosity_, material2->staticRugosity_));

            kineticFrictionCoef_ = BilinearInterpolation(Vector3(1, maxRugosity, reverseSlipperyness2),
                Vector3(maxRugosity, maxRugosity, maxStaticCoeff),
                Vector3(1, 1, Min(reverseSlipperyness1, reverseSlipperyness2)),
                Vector3(maxRugosity, 1, reverseSlipperyness1),
                Vector2(material1->staticRugosity_, material2->kineticRugosity_));



            elasticity_ = Max(material1->elasticity_, material2->elasticity_);
            softness_ = Max(material1->softness_, material2->softness_);



            newtonGroupId0 = material1->newtonGroupId;
            newtonGroupId1 = material2->newtonGroupId;


        }



        float staticFrictionCoef_;
        float kineticFrictionCoef_;

        float elasticity_;
        float softness_;

        int newtonGroupId0 = -1;
        int newtonGroupId1 = -1;
    };
}

