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
/* Desc: Base class for all models.
 * Author: Nathan Koenig and Andrew Howard
 * Date: 8 May 2003
 * SVN: $Id: Model.cc 8494 2010-01-11 19:45:39Z natepak $
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

//#include <boost/python.hpp>

#include <sstream>
#include <iostream>
#include <float.h>

#include "Global.hh"
#include "GazeboError.hh"
#include "GazeboMessage.hh"
#include "XMLConfig.hh"
#include "World.hh"
#include "Simulator.hh"
#include "Body.hh"
#include "HingeJoint.hh"
#include "PhysicsEngine.hh"
#include "Controller.hh"
#include "ControllerFactory.hh"
#include "IfaceFactory.hh"
#include "Model.hh"

#include "rtdb_sim.h"
#include "CoachInfo.h"

using namespace gazebo;

////////////////////////////////////////////////////////////////////////////////
// Constructor
Model::Model(Model *parent)
    : Entity(parent)
{
  this->type = "";
  this->joint = NULL;

  Param::Begin(&this->parameters);
  this->canonicalBodyNameP = new ParamT<std::string>("canonicalBody",
                                                   std::string(),0);

  this->xyzP = new ParamT<Vector3>("xyz", Vector3(0,0,0), 0);
  this->xyzP->Callback(&Entity::SetRelativePosition, (Entity*)this);

  this->rpyP = new ParamT<Quatern>("rpy", Quatern(1,0,0,0), 0);
  this->rpyP->Callback( &Entity::SetRelativeRotation, (Entity*)this);

  this->enableGravityP = new ParamT<bool>("enableGravity", true, 0);
  this->enableGravityP->Callback( &Model::SetGravityMode, this );

  this->enableFrictionP = new ParamT<bool>("enableFriction", true, 0);
  this->enableFrictionP->Callback( &Model::SetFrictionMode, this );

  this->collideP = new ParamT<std::string>("collide", "all", 0);
  this->collideP->Callback( &Model::SetCollideMode, this );

  this->laserFiducialP = new ParamT<int>("laserFiducialId", -1, 0);
  this->laserFiducialP->Callback( &Model::SetLaserFiducialId, this );

  this->laserRetroP = new ParamT<float>("laserRetro", -1, 0);
  this->laserRetroP->Callback( &Model::SetLaserRetro, this );
  
  this->selfID = new ParamT<int>("selfID", -1, 0);
  this->team   = new ParamT<std::string>("team", std::string(), 0);
  
  Param::End();

  this->parentBodyNameP = NULL;
  this->myBodyNameP = NULL;

}

////////////////////////////////////////////////////////////////////////////////
// Destructor
Model::~Model()
{
  std::map< std::string, Body* >::iterator biter;
  JointContainer::iterator jiter;
  std::map< std::string, Controller* >::iterator citer;

  for (biter=this->bodies.begin(); biter != this->bodies.end(); biter++)
  {
    if (biter->second)
    {
      delete biter->second;
      biter->second = NULL;
    }
  }
  this->bodies.clear();

  for (jiter = this->joints.begin(); jiter != this->joints.end(); jiter++)
    if (*jiter)
      delete *jiter;
  this->joints.clear();

  for (citer = this->controllers.begin();
       citer != this->controllers.end(); citer++)
  {
    if (citer->second)
    {
      delete citer->second;
      citer->second = NULL;
    }
  }
  this->controllers.clear();

  if (this->parentBodyNameP)
  {
    delete this->parentBodyNameP;
    this->parentBodyNameP = NULL;
  }

  if (this->myBodyNameP)
  {
    delete this->myBodyNameP;
    this->myBodyNameP = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Load the model
void Model::Load(XMLConfigNode *node, bool removeDuplicate)
{
  XMLConfigNode *childNode;
  std::string scopedName;
  Pose3d pose;
  Model* dup;

  this->nameP->Load(node);

  scopedName = this->GetScopedName();
  
  gzmsg(0) << c_bold << "Loading Model " << c_reset << scopedName << std::endl;

  // Look for existing models by the same name
  if((dup = World::Instance()->GetModelByName(scopedName)) != NULL)
  {
    if(!removeDuplicate)
    {
      gzthrow("Duplicate model name" + scopedName + "\n");
    }
    else
    {
      // Delete the existing one (this should only be reached when called
      // via the factory interface).
      World::Instance()->DeleteEntity(scopedName.c_str());
    }
  }

  this->staticP->Load(node);

  this->canonicalBodyNameP->Load(node);
  this->xyzP->Load(node);
  this->rpyP->Load(node);
  this->enableGravityP->Load(node);
  this->enableFrictionP->Load(node);
  this->collideP->Load(node);
  this->laserFiducialP->Load(node);
  this->laserRetroP->Load(node);
  this->selfID->Load(node);
  this->team->Load(node);

  this->xmlNode = node;
  this->type=node->GetName();

  this->SetStatic( **(this->staticP) );

  // Get the position and orientation of the model (relative to parent)
  pose.Reset();
  pose.pos = **this->xyzP;
  pose.rot = **this->rpyP;

  if (this->IsStatic())
    this->SetRelativePose( pose );

  if (this->type == "physical")
    this->LoadPhysical(node);
  else if (this->type != "empty")
    gzthrow("Invalid model type[" + this->type + "]\n");

  // Set the relative pose of the model
  if (!this->IsStatic())
    this->SetRelativePose( pose );

  // Record the model's initial pose (for reseting)
  this->SetInitPose(pose);

  // Load controllers
  childNode = node->GetChildByNSPrefix("controller");
  while (childNode)
  {
    this->LoadController(childNode);
    childNode = childNode->GetNextByNSPrefix("controller");
  }

  // Create a default body if one does not exist in the XML file
  if (this->bodies.size() <= 0)
  {
    std::ostringstream bodyName;

    bodyName << this->GetName() << "_body";

    // Create an empty body for the model
    Body *body = this->CreateBody();
    body->SetName(bodyName.str());

    // Store the pointer to this body
    this->bodies[body->GetName()] = body;

    this->canonicalBodyNameP->SetValue( bodyName.str() );
  }


  if (this->canonicalBodyNameP->GetValue().empty())
  {
    /// FIXME: Model::pose is set to the pose of first body
    ///        seems like there should be a warning for users
    this->canonicalBodyNameP->SetValue( this->bodies.begin()->first );
  }

  // This must be placed after creation of the bodies
  // Static variable overrides the gravity
  if (**this->staticP == false)
    this->SetGravityMode( **this->enableGravityP );

  //global fiducial and retro id
  if (**this->laserFiducialP != -1.0 )
    this->SetLaserFiducialId(**this->laserFiducialP);

  if (**this->laserRetroP != -1.0)
    this->SetLaserRetro(**this->laserRetroP);

  // Get the name of the python module
  /*this->pName.reset(PyString_FromString(node->GetString("python","",0).c_str()));
  //this->pName.reset(PyString_FromString("pioneer2dx"));

  // Import the python module
  if (this->pName)
  {
  this->pModule.reset(PyImport_Import(this->pName));
  Py_DECREF(this->pName);
  }

  // Get the Update function from the module
  if (this->pModule)
  {
  this->pFuncUpdate.reset(PyObject_GetAttrString(this->pModule, "Update"));
  if (this->pFuncUpdate && !PyCallable_Check(this->pFuncUpdate))
  this->pFuncUpdate = NULL;
  }
   */
}

////////////////////////////////////////////////////////////////////////////////
// Save the model in XML format
void Model::Save(std::string &prefix, std::ostream &stream)
{
  std::string p = prefix + "  ";
  std::string typeName;
  std::map<std::string, Body* >::iterator bodyIter;
  std::map<std::string, Controller* >::iterator contIter;
  JointContainer::iterator jointIter;

  this->xyzP->SetValue( this->GetRelativePose().pos );
  this->rpyP->SetValue( this->GetRelativePose().rot );

  if (this->type=="empty")
    typeName = "empty";
  else if (this->type=="physical")
    typeName = "physical";

  stream << prefix << "<model:" << typeName;
  stream << " name=\"" << this->nameP->GetValue() << "\">\n"; 
  stream << prefix << "  " << *(this->xyzP) << "\n";
  stream << prefix << "  " << *(this->rpyP) << "\n";
  stream << prefix << "  " << *(this->enableGravityP) << "\n";
  stream << prefix << "  " << *(this->enableFrictionP) << "\n";
  stream << prefix << "  " << *(this->collideP) << "\n";

  if (this->type == "physical")
  {
    stream << prefix << "  " << *(this->staticP) << "\n";

    // Save all the bodies
    for (bodyIter=this->bodies.begin(); bodyIter!=this->bodies.end(); bodyIter++)
    {
      stream << "\n";
      if (bodyIter->second)
        bodyIter->second->Save(p, stream);
    }

    // Save all the joints
    for (jointIter = this->joints.begin(); jointIter != this->joints.end(); 
        jointIter++)
      if (*jointIter)
        (*jointIter)->Save(p, stream);

    // Save all the controllers
    for (contIter=this->controllers.begin();
        contIter!=this->controllers.end(); contIter++)
    {
      if (contIter->second)
        contIter->second->Save(p, stream);
    }
  }

  if (this->parentBodyNameP && this->myBodyNameP)
  {
    stream << prefix << "  <attach>\n";
    stream << prefix << "    " << *(this->parentBodyNameP) << "\n";
    stream << prefix << "    " << *(this->myBodyNameP) << "\n";
    stream << prefix << "  </attach>\n";
  }

  // Save all child models
  std::vector< Entity* >::iterator eiter;
  for (eiter = this->children.begin(); eiter != this->children.end(); eiter++)
  {
    Model *cmodel = dynamic_cast<Model*>(*eiter);
    if (cmodel)
      cmodel->Save(p, stream);
  }

  stream << prefix << "</model:" << typeName << ">\n";
}

////////////////////////////////////////////////////////////////////////////////
// Initialize the model
void Model::Init()
{
  
  gzmsg(0) << c_bold << "Initializing Model " << c_reset <<  this->GetScopedName() << std::endl;
    
  std::map<std::string, Body* >::iterator biter;
  std::map<std::string, Controller* >::iterator contIter;

  for (biter = this->bodies.begin(); biter!=this->bodies.end(); biter++)
    biter->second->Init();

  for (contIter=this->controllers.begin();
       contIter!=this->controllers.end(); contIter++)
  {
    contIter->second->Init();
  }

  /*this->mtext = new MovableText(this->GetName(), "this is the caption");
  this->mtext->setTextAlignment(MovableText::H_CENTER, MovableText::V_ABOVE);
  this->mtext->setAdditionalHeight(0.5);
  this->sceneNode->attachObject(this->mtext);
  */

  this->InitChild();
}

////////////////////////////////////////////////////////////////////////////////
// Update the model
void Model::Update()
{
  if (this->controllers.size() == 0 && this->IsStatic())
    return;

  //DiagnosticTimer timer("Model[" + this->GetName() + "] Update ");

#ifdef USE_THREADPOOL
  World::Instance()->GetPhysicsEngine()->InitForThread();
#endif

  std::map<std::string, Body* >::iterator bodyIter;
  std::map<std::string, Controller* >::iterator contIter;
  JointContainer::iterator jointIter;

  Pose3d bodyPose, newPose, oldPose;

  //this->updateSignal();

  {
    //DiagnosticTimer timer("Model[" + this->GetName() + "] Bodies Update ");

    for (bodyIter=this->bodies.begin(); 
         bodyIter!=this->bodies.end(); bodyIter++)
    {
      if (bodyIter->second)
      {
#ifdef USE_THREADPOOL
        World::Instance()->threadPool->schedule(
            boost::bind(&Body::Update,(bodyIter->second)));
#else
        bodyIter->second->Update();
#endif
      }
    }
  }

  {
    //DiagnosticTimer timer("Model[" + this->GetName() + "] Controllers Update ");
    for (contIter=this->controllers.begin();
        contIter!=this->controllers.end(); contIter++)
    {
      if (contIter->second)
      {
#ifdef USE_THREADPOOL
        World::Instance()->threadPool->schedule(boost::bind(&Controller::Update,(contIter->second)));
#else
        contIter->second->Update();
#endif
      }
    }
  }



  // Call the model's python update function, if one exists
  /*if (this->pFuncUpdate)
  {
    boost::python::call<void>(this->pFuncUpdate, this);
  }*/

  // BULLET:
  if (!this->canonicalBodyNameP->GetValue().empty())
  {
    /// model pose is the canonical body pose of the body + a transform from body frame to model frame
    /// the tranform is defined by initModelOffset in body frame,
    
    this->SetAbsPose( this->bodies[**this->canonicalBodyNameP]->GetAbsPose(), false );
    
    /*
    /// recover the transform in inertial frame based on body pose
    this->pose = this->bodies[**this->canonicalBodyNameP]->GetPose();
    Quatern body_rot = this->pose.rot;
    Pose3d offset_transform = this->bodies[**this->canonicalBodyNameP]->initModelOffset;
    Vector3 xyz_offset = (offset_transform.RotatePositionAboutOrigin(body_rot.GetInverse())).pos;
    Quatern q_offset = offset_transform.rot;

    // apply transform to get model pose
    this->pose.pos = this->pose.pos + xyz_offset;
    this->pose.rot = this->pose.CoordRotationAdd(q_offset);

    this->xyzP->SetValue(this->pose.pos);
    this->rpyP->SetValue(this->pose.rot);
    */
  }

  {
    //DiagnosticTimer timer("Model[" + this->GetName() + "] Children Update ");
    this->UpdateChild();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Finalize the model
void Model::Fini()
{
  std::map<std::string, Body* >::iterator biter;
  std::map<std::string, Controller* >::iterator contIter;

  for (contIter = this->controllers.begin();
       contIter != this->controllers.end(); contIter++)
  {
    contIter->second->Fini();
  }

  for (biter=this->bodies.begin(); biter != this->bodies.end(); biter++)
  {
    biter->second->Fini();
  }

  this->FiniChild();
}

////////////////////////////////////////////////////////////////////////////////
// Reset the model
void Model::Reset()
{
  JointContainer::iterator jiter;
  std::map< std::string, Body* >::iterator biter;
  std::map<std::string, Controller* >::iterator citer;
  Vector3 v(0,0,0);

//  this->SetPose(this->initPose);

  for (citer=this->controllers.begin(); citer!=this->controllers.end(); citer++)
  {
    citer->second->Reset();
  }

  for (jiter=this->joints.begin(); jiter!=this->joints.end(); jiter++)
  {
    (*jiter)->Reset();
  }

  for (biter=this->bodies.begin(); biter != this->bodies.end(); biter++)
  {
    biter->second->SetLinearVel(v);
    biter->second->SetAngularVel(v);
    biter->second->SetForce(v);
    biter->second->SetTorque(v);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Restore the model...
void Model::Restore(){

    if ( this->GetSelfID() == - 1 ) return;

    CoachInfo cInfo;
    // Fetch coach data
    DB_get_from(0, 0, COACH_INFO, (void*)&cInfo);
    cInfo.changePositionSN[ this->GetSelfID() - 1 ]++;
    DB_put_in(0, 0, COACH_INFO, (void*)&cInfo, 0);

}


////////////////////////////////////////////////////////////////////////////////
// Get the name of the model
const std::string &Model::GetType() const
{
  return this->type;
}
////////////////////////////////////////////////////////////////////////////////
// Set the initial pose
void Model::SetInitPose(const Pose3d &pose)
{
  this->initPose = pose;
}

////////////////////////////////////////////////////////////////////////////////
// Get the initial pose
const Pose3d &Model::GetInitPose() const
{
  return this->initPose;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the linear velocity of the model
void Model::SetLinearVel( const Vector3 &vel )
{
  Body *body;
  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetLinearVel( vel );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the angular velocity of the model
void Model::SetAngularVel( const Vector3 &vel )
{
  Body *body;
  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetAngularVel( vel );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the linear acceleration of the model
void Model::SetLinearAccel( const Vector3 &accel )
{
  Body *body;
  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetLinearAccel( accel );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the angular acceleration of the model
void Model::SetAngularAccel( const Vector3 &accel )
{
  Body *body;
  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetAngularAccel( accel );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Get the linear velocity of the model
Vector3 Model::GetLinearVel() const
{
  std::map<std::string, Body* >::const_iterator iter;
  iter = this->bodies.begin();
  return iter->second->GetLinearVel();
}

////////////////////////////////////////////////////////////////////////////////
/// Get the angular velocity of the model
Vector3 Model::GetAngularVel() const
{
  std::map<std::string, Body* >::const_iterator iter;
  iter = this->bodies.begin();
  return iter->second->GetAngularVel();
}

////////////////////////////////////////////////////////////////////////////////
/// Get the linear acceleration of the model
Vector3 Model::GetLinearAccel() const
{
  std::map<std::string, Body* >::const_iterator iter;
  iter = this->bodies.begin();
  return iter->second->GetLinearAccel();
}

////////////////////////////////////////////////////////////////////////////////
/// Get the angular acceleration of the model
Vector3 Model::GetAngularAccel() const
{
  std::map<std::string, Body* >::const_iterator iter;
  iter = this->bodies.begin();
  return iter->second->GetAngularAccel();
}

////////////////////////////////////////////////////////////////////////////////
/// Get the size of the bounding box
void Model::GetBoundingBox(Vector3 &min, Vector3 &max) const
{
  Vector3 bbmin, bbmax;
  std::map<std::string, Body* >::const_iterator iter;

  if (this->type == "empty"){
    min.Set(0.0, 0.0, 0.0);
    max.Set(0.0, 0.0, 0.0);
    return;
  }
  
  min.Set(FLT_MAX, FLT_MAX, FLT_MAX);
  max.Set(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    iter->second->GetBoundingBox(bbmin, bbmax);
    min.x = std::min(bbmin.x, min.x);
    min.y = std::min(bbmin.y, min.y);
    min.z = std::min(bbmin.z, min.z);

    max.x = std::max(bbmax.x, max.x);
    max.y = std::max(bbmax.y, max.y);
    max.z = std::max(bbmax.z, max.z);
  }
}
 
////////////////////////////////////////////////////////////////////////////////
// Create and return a new body
Body *Model::CreateBody()
{
  // Create a new body
  return World::Instance()->GetPhysicsEngine()->CreateBody(this);
}

////////////////////////////////////////////////////////////////////////////////
/// Get the number of joints
unsigned int Model::GetJointCount() const
{
  return this->joints.size();
}

////////////////////////////////////////////////////////////////////////////////
/// Get a joing by index
Joint *Model::GetJoint( unsigned int index ) const
{
  if (index >= this->joints.size())
    gzthrow("Invalid joint index[" << index << "]\n");

  return this->joints[index];
}

////////////////////////////////////////////////////////////////////////////////
// Get a joint by name
Joint *Model::GetJoint(std::string name)
{
  JointContainer::iterator iter;

  for (iter = this->joints.begin(); iter != this->joints.end(); iter++)
    if ( (*iter)->GetName() == name)
      return (*iter);

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Load a new body helper function
void Model::LoadBody(XMLConfigNode *node)
{
  if (!node)
    gzthrow("Trying to load a body with NULL XML information");

  // Create a new body
  Body *body = this->CreateBody();

  // Load the body using the config node. This also loads all of the
  // bodies geometries
  body->Load(node);

  // Store this body
  if (this->bodies[body->GetName()])
    gzmsg(0) << "Body with name[" << body->GetName() << "] already exists!!\n";

  // Store the pointer to this body
  this->bodies[body->GetName()] = body;
}

////////////////////////////////////////////////////////////////////////////////
// Load a new joint helper function
void Model::LoadJoint(XMLConfigNode *node)
{
  if (!node)
    gzthrow("Trying to load a joint with NULL XML information");

  Joint *joint;
  Joint::Type jtype;

  // Create a Hinge Joint
  if (node->GetName() == "hinge")
    jtype = Joint::HINGE;
  else if (node->GetName() == "ball")
    jtype = Joint::BALL;
  else if (node->GetName() == "slider")
    jtype = Joint::SLIDER;
  else if (node->GetName() == "hinge2")
    jtype = Joint::HINGE2;
  else if (node->GetName() == "universal")
    jtype = Joint::UNIVERSAL;
  else
  {
    gzthrow("Uknown joint[" + node->GetName() + "]\n");
  }

  joint = World::Instance()->GetPhysicsEngine()->CreateJoint(jtype);

  joint->SetModel(this);

  // Load each joint
  joint->Load(node);

  if (this->GetJoint( joint->GetName() ) != NULL)
    gzthrow( "can't have two joint with the same name");

  this->joints.push_back( joint );
  
}

////////////////////////////////////////////////////////////////////////////////
/// Load a controller helper function
void Model::LoadController(XMLConfigNode *node)
{
  if (!node)
    gzthrow( "node parameter is NULL" );

  Controller *controller=0;
  std::ostringstream stream;

  // Get the controller's type
  std::string controllerType = node->GetName();

  // Get the unique name of the controller
  std::string controllerName = node->GetString("name",std::string(),1);
  
  // See if the controller is in a plugin
  std::string pluginName = node->GetString("plugin","",0);
  if (pluginName != "")
	  ControllerFactory::LoadPlugin(pluginName, controllerType);

  // Create the controller based on it's type
  controller = ControllerFactory::NewController(controllerType, this);

  if (controller)
  {
    // Load the controller
    try
    {
      controller->Load(node);
    }
    catch (GazeboError e)
    {
      gzerr(0) << "Error Loading Controller[" <<  controllerName
      << "]\n" << e << std::endl;
      delete controller;
      controller = NULL;
      return;
    }

    // Store the controller
    this->controllers[controllerName] = controller;
  }
  else
  {
    gzmsg(0) << "Unknown controller[" << controllerType << "]\n";
  }
}

////////////////////////////////////////////////////////////////////////////////
// Return the default body
Body *Model::GetBody()
{
  return this->bodies.begin()->second;
}

////////////////////////////////////////////////////////////////////////////////
/// Get a map of all the bodies
const std::map<std::string, Body*> *Model::GetBodies() const
{
  return &(this->bodies);
}

////////////////////////////////////////////////////////////////////////////////
/// Get a sensor by name
Sensor *Model::GetSensor(const std::string &name) const
{
  Sensor *sensor = NULL;
  std::map< std::string, Body* >::const_iterator biter;

  for (biter=this->bodies.begin(); biter != this->bodies.end(); biter++)
  {
    if ( (sensor = biter->second->GetSensor(name)) != NULL)
      break;
  }

  return sensor;
}
 
////////////////////////////////////////////////////////////////////////////////
/// Get a geom by name
Geom *Model::GetGeom(const std::string &name) const
{
  Geom *geom = NULL;
  std::map< std::string, Body* >::const_iterator biter;

  for (biter=this->bodies.begin(); biter != this->bodies.end(); biter++)
  {
    if ( (geom = biter->second->GetGeom(name)) != NULL)
      break;
  }

  return geom;
}

////////////////////////////////////////////////////////////////////////////////
/// Get a body by name
Body *Model::GetBody(const std::string &name)
{
  if (this->bodies.find(name) != this->bodies.end())
  {
    return this->bodies[name];
  }
  else if (name == "canonical")
  {
    return this->GetCanonicalBody();
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Attach this model to its parent
void Model::Attach(XMLConfigNode *node)
{
  Model *parentModel = NULL;

  Param::Begin(&this->parameters);
  this->parentBodyNameP = new ParamT<std::string>("parentBody","canonical",1);
  this->myBodyNameP = new ParamT<std::string>("myBody",this->canonicalBodyNameP->GetValue(),1);
  Param::End();

  if (node)
  {
    this->parentBodyNameP->Load(node);
    this->myBodyNameP->Load(node);
  }

  parentModel = dynamic_cast<Model*>(this->parent);

  if (parentModel == NULL)
    gzthrow("Parent cannot be NULL when attaching two models");

  this->joint =World::Instance()->GetPhysicsEngine()->CreateJoint(Joint::HINGE);

  Body *myBody = this->GetBody(**(this->myBodyNameP));
  Body *pBody = parentModel->GetBody(**(this->parentBodyNameP));

  if (myBody == NULL)
    gzthrow("No canonical body set.");

  if (pBody == NULL)
    gzthrow("Parent has no canonical body");

  this->joint->Attach(myBody, pBody);
  this->joint->SetAnchor(0, myBody->GetAbsPose().pos );
  this->joint->SetAxis(0, Vector3(0,1,0) );
  this->joint->SetHighStop(0,Angle(0));
  this->joint->SetLowStop(0,Angle(0));
}

////////////////////////////////////////////////////////////////////////////////
/// Get the canonical body. Used for connected Model heirarchies
Body *Model::GetCanonicalBody()
{
  return this->bodies[this->canonicalBodyNameP->GetValue()];
}

////////////////////////////////////////////////////////////////////////////////
/// Set the gravity mode of the model
void Model::SetGravityMode( const bool &v )
{
  Body *body;
  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetGravityMode( v );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the gravity mode of the model
void Model::SetFrictionMode( const bool &v )
{
  Body *body;

  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetFrictionMode( v );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the collide mode of the model
void Model::SetCollideMode( const std::string &m )
{
  Body *body;

  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetCollideMode( m );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Set the Fiducial Id of the model
void Model::SetLaserFiducialId( const int &id )
{
  Body *body;
  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetLaserFiducialId( id );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Get the Fiducial Id of the Model
int Model::GetLaserFiducialId( )
{
 //this is not correct if geoms set their own Fiducial. 
 //you can not expect it to be correct in that case anyway ...
  return **this->laserFiducialP; 
}

////////////////////////////////////////////////////////////////////////////////
/// Set the Laser retro property of the model
void Model::SetLaserRetro( const float &retro )
{
  Body *body;

  std::map<std::string, Body* >::iterator iter;

  for (iter=this->bodies.begin(); iter!=this->bodies.end(); iter++)
  {
    body = iter->second;

    body->SetLaserRetro( retro );
  }
}

////////////////////////////////////////////////////////////////////////////////
// Load a physical model
void Model::LoadPhysical(XMLConfigNode *node)
{
  XMLConfigNode *childNode = NULL;

  // Load the bodies
  if (node->GetChildByNSPrefix("body"))
    childNode = node->GetChildByNSPrefix("body");
  else
    childNode = node->GetChild("body");

  while (childNode)
  {
    this->LoadBody(childNode);

    if (childNode->GetNextByNSPrefix("body"))
      childNode = childNode->GetNextByNSPrefix("body");
    else
      childNode = childNode->GetNext("body");
  }

  // Load the joints
  childNode = node->GetChildByNSPrefix("joint");

  while (childNode)
  {
    try
    {
      this->LoadJoint(childNode);
    }
    catch (GazeboError e)
    {
      gzerr(0) << "Error Loading Joint[" << childNode->GetString("name", std::string(), 0) << "]\n";
      gzerr(0) <<  e << std::endl;
      childNode = childNode->GetNextByNSPrefix("joint");
      continue;
    }
    childNode = childNode->GetNextByNSPrefix("joint");
  }
}


////////////////////////////////////////////////////////////////////////////////
/// Get the list of model interfaces 
/// e.g "pioneer2dx_model1::laser::laser_iface0->laser"
void Model::GetModelInterfaceNames(std::vector<std::string>& list) const
{
  std::map< std::string, Body* >::const_iterator biter;
  std::map<std::string, Controller* >::const_iterator contIter;

  for (contIter=this->controllers.begin();
      contIter!=this->controllers.end(); contIter++)
  {
    contIter->second->GetInterfaceNames(list);	

  }

  for (biter=this->bodies.begin(); biter != this->bodies.end(); biter++)
  {

    biter->second->GetInterfaceNames(list);
  }
}

////////////////////////////////////////////////////////////////////////////////
int Model::GetSelfID() const {
  return this->selfID->GetValue();
}

////////////////////////////////////////////////////////////////////////////////
std::string Model::GetTeam() const {
  return this->team->GetValue();
}


