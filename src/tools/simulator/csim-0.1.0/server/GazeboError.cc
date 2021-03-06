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
/*
 * Desc: Gazebo Error
 * Author: Nathan Koenig
 * Date: 07 May 2007
 * SVN info: $Id: GazeboError.cc 7042 2008-09-24 18:09:46Z natepak $
 */

#include "GazeboError.hh"

using namespace gazebo;
using namespace std;


////////////////////////////////////////////////////////////////////////////////
/// Default constructor
GazeboError::GazeboError(const char *file, int line, std::string msg)
{
  this->file = file;
  this->line = line;
  this->str = msg;
}

////////////////////////////////////////////////////////////////////////////////
/// Destructor
GazeboError::~GazeboError()
{
}

////////////////////////////////////////////////////////////////////////////////
/// Return the error file
std::string GazeboError::GetErrorFile() const
{
  return this->file;
}

////////////////////////////////////////////////////////////////////////////////
/// Return the error string
std::string GazeboError::GetErrorStr() const
{
  return this->str;
}

////////////////////////////////////////////////////////////////////////////////
/// Return the error code
int GazeboError::GetErrorLine() const
{
  return this->line;
}

