/*  Copyright 2011, 2012 The Ready Bunch

    This file is part of Ready.

    Ready is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Ready is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Ready. If not, see <http://www.gnu.org/licenses/>.         */

// local:
#include "overlays.hpp"
#include "BaseRD.hpp"
#include "utils.hpp"

// VTK:
#include <vtkXMLDataElement.h>
#include <vtkImageData.h>

// STL:
#include <stdexcept>
using namespace std;

class Point3D : public XML_Object
{
    public:
    
        Point3D(vtkXMLDataElement *node);
        
        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const;

        static const char* GetTypeName() { return "Point3D"; }

    public:
    
        float x,y,z;
};

class BaseOperation : public Reusable_XML_Object
{
    public:

        // construct when we don't know the derived type
        static BaseOperation* New(vtkXMLDataElement* node);

        virtual void Apply(float& target,float value) const =0;

    protected:
        
        // can construct from an XML node
        BaseOperation(vtkXMLDataElement* node) : Reusable_XML_Object(node) {}
};

class BaseFill : public Reusable_XML_Object
{
    public:

        // construct when we don't know the derived type
        static BaseFill* New(vtkXMLDataElement* node);

        // what value would this fill type be at the given location, given the existing image as in system
        virtual float GetValue(BaseRD *system,int x,int y,int z) const =0;

    protected:

        // can construct from an XML node
        BaseFill(vtkXMLDataElement* node) : Reusable_XML_Object(node) {}
};
 
class BaseShape : public Reusable_XML_Object
{
    public:

        // construct when we don't know the derived type
        static BaseShape* New(vtkXMLDataElement* node);

        virtual bool IsInside(float x,float y,float z,int dimensionality) const =0;

    protected:

        // can construct from an XML node
        BaseShape(vtkXMLDataElement* node) : Reusable_XML_Object(node) {}
};

Point3D::Point3D(vtkXMLDataElement *node)
{
    read_required_attribute(node,"x",this->x);
    read_required_attribute(node,"y",this->y);
    read_required_attribute(node,"z",this->z);
}

vtkSmartPointer<vtkXMLDataElement> Point3D::GetAsXML() const
{
    vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
    xml->SetName(Point3D::GetTypeName());
    xml->SetFloatAttribute("x",this->x);
    xml->SetFloatAttribute("y",this->y);
    xml->SetFloatAttribute("z",this->z);
    return xml;
}

Overlay::Overlay(vtkXMLDataElement* node)
{
    char c;
    read_required_attribute(node,"chemical",c);
    this->iTargetChemical = c-'a';
    if(node->GetNumberOfNestedElements()!=3)
        throw runtime_error("overlay : expected 3 nested elements (operation, fill, shape)");
    this->op = BaseOperation::New(node->GetNestedElement(0));
    this->fill = BaseFill::New(node->GetNestedElement(1));
    this->shape = BaseShape::New(node->GetNestedElement(2));
}

vtkSmartPointer<vtkXMLDataElement> Overlay::GetAsXML() const
{
    vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
    xml->SetName(Overlay::GetTypeName());
    xml->SetAttribute("chemical",string(1,'a'+this->iTargetChemical).c_str());
    xml->AddNestedElement(this->op->GetAsXML());
    xml->AddNestedElement(this->fill->GetAsXML());
    xml->AddNestedElement(this->shape->GetAsXML());
    return xml;
}

Reusable_XML_Object::Reusable_XML_Object(vtkXMLDataElement *node)
{
    /* TODO: this is not yet functional!
    const char *s;
    s = node->GetAttribute("label");
    if(s)
    {
        from_string(s,this->label);
        // TODO: add this instance to some external list
    }
    s = node->GetAttribute("ref");
    if(s)
    {
        string ref;
        from_string(s,ref);
        // TODO: load the previous instance
    }
    */
}

void Overlay::Apply(BaseRD* system,int x,int y,int z) const
{
    if(this->shape->IsInside(x/float(system->GetX()),y/float(system->GetY()),z/float(system->GetZ()),system->GetDimensionality()))
        this->op->Apply(*vtk_at(static_cast<float*>(system->GetImage(this->iTargetChemical)->GetScalarPointer()),x,y,z,system->GetX(),system->GetY()),
            this->fill->GetValue(system,x,y,z));
}

// -------------------------- the derived types ----------------------------------

class Add : public BaseOperation
{
    public:

        Add(vtkXMLDataElement* node) : BaseOperation(node) {}

        static const char* GetTypeName() { return "add"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(Add::GetTypeName());
            return xml;
        }

        virtual void Apply(float& target,float value) const { target += value; }
};

class Subtract : public BaseOperation
{
    public:

        Subtract(vtkXMLDataElement* node) : BaseOperation(node) {}

        static const char* GetTypeName() { return "subtract"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(Subtract::GetTypeName());
            return xml;
        }

        virtual void Apply(float& target,float value) const { target -= value; }
};

class Overwrite : public BaseOperation
{
    public:

        Overwrite(vtkXMLDataElement* node) : BaseOperation(node) {}

        static const char* GetTypeName() { return "overwrite"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(Overwrite::GetTypeName());
            return xml;
        }

        virtual void Apply(float& target,float value) const { target = value; }
};

class Multiply : public BaseOperation
{
    public:

        Multiply(vtkXMLDataElement* node) : BaseOperation(node) {}

        static const char* GetTypeName() { return "multiply"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(Multiply::GetTypeName());
            return xml;
        }

        virtual void Apply(float& target,float value) const { target *= value; }
};

class Constant : public BaseFill
{
    public:

        Constant(vtkXMLDataElement* node) : BaseFill(node)
        {
            read_required_attribute(node,"value",this->value);
        }

        static const char* GetTypeName() { return "constant"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(Constant::GetTypeName());
            xml->SetFloatAttribute("value",this->value);
            return xml;
        }

        virtual float GetValue(BaseRD *system,int x,int y,int z) const
        {
            return this->value;
        }

    protected:

        float value;
};

class OtherChemical : public BaseFill
{
    public:

        OtherChemical(vtkXMLDataElement* node) : BaseFill(node)
        {
            char c;
            read_required_attribute(node,"chemical",c);
            this->iOtherChemical = c-'a';
        }

        static const char* GetTypeName() { return "other_chemical"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(OtherChemical::GetTypeName());
            xml->SetAttribute("chemical",string(1,'a'+this->iOtherChemical).c_str());
            return xml;
        }

        virtual float GetValue(BaseRD *system,int x,int y,int z) const
        {
            return *vtk_at(static_cast<float*>(system->GetImage(this->iOtherChemical)->GetScalarPointer()),
                x,y,z,system->GetX(),system->GetY());
        }

    protected:

        int iOtherChemical;
};

class WhiteNoise : public BaseFill
{
    public:

        WhiteNoise(vtkXMLDataElement* node) : BaseFill(node)
        {
            read_required_attribute(node,"low",this->low);
            read_required_attribute(node,"high",this->high);
        }

        static const char* GetTypeName() { return "white_noise"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(WhiteNoise::GetTypeName());
            xml->SetFloatAttribute("low",this->low);
            xml->SetFloatAttribute("high",this->high);
            return xml;
        }

        virtual float GetValue(BaseRD *system,int x,int y,int z) const
        {
            return frand(this->low,this->high);
        }

    protected:

        float low,high;
};

class Everywhere : public BaseShape
{
    public:

        Everywhere(vtkXMLDataElement* node) : BaseShape(node) {}

        static const char* GetTypeName() { return "everywhere"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(Everywhere::GetTypeName());
            return xml;
        }

        virtual bool IsInside(float x,float y,float z,int dimensionality) const { return true; }
};

class Rectangle : public BaseShape
{
    public:

        Rectangle(vtkXMLDataElement* node) : BaseShape(node),a(node->GetNestedElement(0)),b(node->GetNestedElement(1))
        {
        }

        static const char* GetTypeName() { return "rectangle"; }

        virtual vtkSmartPointer<vtkXMLDataElement> GetAsXML() const
        {
            vtkSmartPointer<vtkXMLDataElement> xml = vtkSmartPointer<vtkXMLDataElement>::New();
            xml->SetName(Rectangle::GetTypeName());
            xml->AddNestedElement(a.GetAsXML());
            xml->AddNestedElement(b.GetAsXML());
            return xml;
        }

        virtual bool IsInside(float x,float y,float z,int dimensionality) const
        {
            switch(dimensionality)
            {
                case 1: return x>=this->a.x && x<=this->b.x;
                case 2: return x>=this->a.x && x<=this->b.x && y>=this->a.y && y<=this->b.y;
                case 3: return x>=this->a.x && x<=this->b.x && y>=this->a.y && y<=this->b.y && z>=this->a.z && z<=this->b.z;
                default: throw runtime_error("Rectangle::IsInside : unsupported dimensionality");
            }
        }

    protected:

        Point3D a,b;
};

// -------------------------------------------------------------------------------

// when you create a new derived class, add it to the appropriate factory method here:

/* static */ BaseOperation* BaseOperation::New(vtkXMLDataElement *node)
{
    string name(node->GetName());
    if(name==Overwrite::GetTypeName())         return new Overwrite(node);
    else if(name==Add::GetTypeName())          return new Add(node);
    else if(name==Subtract::GetTypeName())     return new Subtract(node);
    else if(name==Multiply::GetTypeName())     return new Multiply(node);
    else throw runtime_error("Unsupported operation: "+name);
}

/* static */ BaseFill* BaseFill::New(vtkXMLDataElement* node)
{
    string name(node->GetName());
    if(name==Constant::GetTypeName())             return new Constant(node);
    else if(name==WhiteNoise::GetTypeName())      return new WhiteNoise(node);
    else if(name==OtherChemical::GetTypeName())   return new OtherChemical(node);
    else throw runtime_error("Unsupported fill type: "+name);
}

/* static */ BaseShape* BaseShape::New(vtkXMLDataElement* node)
{
    string name(node->GetName());
    if(name==Everywhere::GetTypeName())        return new Everywhere(node);
    else if(name==Rectangle::GetTypeName())    return new Rectangle(node);
    else throw runtime_error("Unsupported shape: "+name);
}