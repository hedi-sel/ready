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
#include "GrayScott.hpp"
#include "utils.hpp"

// stdlib:
#include <stdlib.h>
#include <math.h>

// STL:
#include <stdexcept>
using namespace std;

// VTK:
#include <vtkImageData.h>

GrayScott::GrayScott()
{
    this->rule_name = "Gray-Scott";
    this->n_chemicals = 2;
    this->AddParameter("timestep",1.0f);
    this->AddParameter("D_a",0.082f);
    this->AddParameter("D_b",0.041f);
    // for spots:
    this->AddParameter("k",0.064f);
    this->AddParameter("F",0.035f);
}

void GrayScott::AllocateImages(int x,int y,int z,int nc)
{
    if(nc!=2) throw runtime_error("GrayScott::Allocate : this implementation is for 2 chemicals only"); 
    ImageRD::AllocateImages(x,y,z,2);
    // also allocate our buffer images
    this->DeleteBuffers();
    this->buffer_images.resize(2);
    for(int i=0;i<2;i++)
        this->buffer_images[i] = AllocateVTKImage(x,y,z);
}

GrayScott::~GrayScott()
{
    this->DeleteBuffers();
}

void GrayScott::DeleteBuffers()
{
    for(int i=0;i<(int)this->buffer_images.size();i++)
    {
        if(this->buffer_images[i])
            this->buffer_images[i]->Delete();
    }
}

void GrayScott::InternalUpdate(int n_steps)
{
    const int X = this->GetX();
    const int Y = this->GetY();
    const int Z = this->GetZ();

    float timestep = this->GetParameterValueByName("timestep");
    float D_a = this->GetParameterValueByName("D_a");
    float D_b = this->GetParameterValueByName("D_b");
    float k = this->GetParameterValueByName("k");
    float F = this->GetParameterValueByName("F");

    // take approximately n_steps
    for(int iStep=0;iStep<n_steps;iStep++)
    {
        float *old_a,*new_a,*old_b,*new_b;
        switch(iStep%2) 
        {
            case 0: old_a = static_cast<float*>(this->images[0]->GetScalarPointer()); 
                    old_b = static_cast<float*>(this->images[1]->GetScalarPointer()); 
                    new_a = static_cast<float*>(this->buffer_images[0]->GetScalarPointer()); 
                    new_b = static_cast<float*>(this->buffer_images[1]->GetScalarPointer()); 
                    break;
            case 1: old_a = static_cast<float*>(this->buffer_images[0]->GetScalarPointer()); 
                    old_b = static_cast<float*>(this->buffer_images[1]->GetScalarPointer()); 
                    new_a = static_cast<float*>(this->images[0]->GetScalarPointer()); 
                    new_b = static_cast<float*>(this->images[1]->GetScalarPointer()); 
                    break;
        }
        for(int z=0;z<Z;z++)
        {
            int z_prev = (z-1+Z)%Z;
            int z_next = (z+1)%Z;
            for(int y=0;y<Y;y++)
            {
                int y_prev = (y-1+Y)%Y;
                int y_next = (y+1)%Y;
                for(int x=0;x<X;x++)
                {
                    int x_prev = (x-1+X)%X;
                    int x_next = (x+1)%X;

                    float aval = *vtk_at(old_a,x,y,z,X,Y);
                    float bval = *vtk_at(old_b,x,y,z,X,Y);

                    // compute the Laplacians of a and b
                    // 7-point stencil:
                    float dda = *vtk_at(old_a,x,y_prev,z,X,Y) +
                                *vtk_at(old_a,x,y_next,z,X,Y) +
                                *vtk_at(old_a,x_prev,y,z,X,Y) + 
                                *vtk_at(old_a,x_next,y,z,X,Y) +
                                *vtk_at(old_a,x,y,z_prev,X,Y) + 
                                *vtk_at(old_a,x,y,z_next,X,Y) - 6*aval;
                    float ddb = *vtk_at(old_b,x,y_prev,z,X,Y) +
                                *vtk_at(old_b,x,y_next,z,X,Y) +
                                *vtk_at(old_b,x_prev,y,z,X,Y) + 
                                *vtk_at(old_b,x_next,y,z,X,Y) +
                                *vtk_at(old_b,x,y,z_prev,X,Y) + 
                                *vtk_at(old_b,x,y,z_next,X,Y) - 6*bval;
     
                    // compute the new rate of change of a and b
                    float da = D_a * dda - aval*bval*bval + F*(1-aval);
                    float db = D_b * ddb + aval*bval*bval - (F+k)*bval;

                    // apply the change
                    *vtk_at(new_a,x,y,z,X,Y) = aval + timestep * da;
                    *vtk_at(new_b,x,y,z,X,Y) = bval + timestep * db;
                }
            }
        }
    }
    if(n_steps%2)
    {
        // output ended up in the buffer images
        this->images[0]->DeepCopy(this->buffer_images[0]);
        this->images[1]->DeepCopy(this->buffer_images[1]);
    }
}
