/*
 *  Gazebo - Outdoor Multi-Robot Simulator
 *  Copyright (C) 2003
 *     Nate Koenig & Andrew Howard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/* Desc: A ODEHingeJoint
 * Author: Nate Keonig, Andrew Howard
 * Date: 21 May 2003
 * CVS: $Id: ODEHingeJoint.cc 7640 2009-05-13 02:06:08Z natepak $
 *
 * Modified by: Eurico Pedrosa <efp@ua.p>
 * Date: 10 Fev 2010
 *
 * Modification Notes
 *
 *  The modifications presented by me, have the purpose of
 *  removing the 'rendering' and 'gui' modules from the code base.
 *  The reasons behind this decision are simple, allow gazebo to run
 *  on computers with less gpu capabilities and lessen the the coupling
 *  between simulation and visualization.
 *
 */

#include "Model.hh"
#include "Body.hh"
#include "World.hh"
#include "XMLConfig.hh"
#include "Global.hh"
#include "ODEHingeJoint.hh"


using namespace gazebo;

//////////////////////////////////////////////////////////////////////////////
// Constructor
ODEHingeJoint::ODEHingeJoint( dWorldID worldId )
    : HingeJoint<ODEJoint>()
{
  this->jointId = dJointCreateHinge(worldId, NULL);
}

//////////////////////////////////////////////////////////////////////////////
// Destructor
ODEHingeJoint::~ODEHingeJoint()
{
}

//////////////////////////////////////////////////////////////////////////////
/// Load a hinge joint
void ODEHingeJoint::Load(XMLConfigNode *node)
{
  HingeJoint<ODEJoint>::Load(node);

  this->SetParam(dParamFMax, 0);
  this->SetForce(0, 0);
}

//////////////////////////////////////////////////////////////////////////////
// Get the anchor point
Vector3 ODEHingeJoint::GetAnchor(int /*index*/) const
{
  dVector3 result;

  this->physics->LockMutex();
  dJointGetHingeAnchor( this->jointId, result );
  this->physics->UnlockMutex();

  return Vector3(result[0], result[1], result[2]);
}

//////////////////////////////////////////////////////////////////////////////
// Set the anchor point
void ODEHingeJoint::SetAnchor( int /*index*/, const Vector3 &anchor )
{
  this->physics->LockMutex();
  dJointSetHingeAnchor( this->jointId, anchor.x, anchor.y, anchor.z );
  this->physics->UnlockMutex();
}


//////////////////////////////////////////////////////////////////////////////
// Get the axis of rotation
Vector3 ODEHingeJoint::GetAxis(int /*index*/) const
{
  dVector3 result;

  this->physics->LockMutex();
  dJointGetHingeAxis( this->jointId, result );
  this->physics->UnlockMutex();

  return Vector3(result[0], result[1], result[2]);
}

//////////////////////////////////////////////////////////////////////////////
// Set the axis of rotation
void ODEHingeJoint::SetAxis( int /*index*/, const Vector3 &axis )
{
  this->physics->LockMutex();
  dJointSetHingeAxis( this->jointId, axis.x, axis.y, axis.z );
  this->physics->UnlockMutex();
}

//////////////////////////////////////////////////////////////////////////////
// Get the angle of rotation
Angle ODEHingeJoint::GetAngle(int /*index*/) const
{
  this->physics->LockMutex();
  Angle result = dJointGetHingeAngle( this->jointId );
  this->physics->UnlockMutex();

  return result;
}

//////////////////////////////////////////////////////////////////////////////
// Get the rotation rate
double ODEHingeJoint::GetVelocity(int /*index*/) const
{
  this->physics->LockMutex();
  double result = dJointGetHingeAngleRate( this->jointId );
  this->physics->UnlockMutex();

  return result;
} 

//////////////////////////////////////////////////////////////////////////////
/// Set the velocity of an axis(index).
void ODEHingeJoint::SetVelocity(int /*index*/, double angle)
{
  this->SetParam(dParamVel, angle);
}

//////////////////////////////////////////////////////////////////////////////
/// Set the max allowed force of an axis(index).
void ODEHingeJoint::SetMaxForce(int /*index*/, double t)
{
  return this->SetParam(dParamFMax, t);
}

//////////////////////////////////////////////////////////////////////////////
/// Get the max allowed force of an axis(index).
double ODEHingeJoint::GetMaxForce(int /*index*/)
{
  return this->GetParam(dParamFMax);
}

//////////////////////////////////////////////////////////////////////////////
// Set the torque of this joint
void ODEHingeJoint::SetForce(int /*index*/, double torque)
{
  this->physics->LockMutex();
  dJointAddHingeTorque( this->jointId, torque );
  this->physics->UnlockMutex();
}

//////////////////////////////////////////////////////////////////////////////
// Get the specified parameter
double ODEHingeJoint::GetParam( int parameter ) const
{
  this->physics->LockMutex();
  double result = dJointGetHingeParam( this->jointId, parameter );
  this->physics->UnlockMutex();

  return result; 
}

//////////////////////////////////////////////////////////////////////////////
// Set the _parameter to _value
void ODEHingeJoint::SetParam( int parameter, double value)
{
  this->physics->LockMutex();
  dJointSetHingeParam( this->jointId, parameter, value );
  this->physics->UnlockMutex();
}
