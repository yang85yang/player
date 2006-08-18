/*
 *  Player - One Hell of a Robot Server
 *  Copyright (C) 2000  Brian Gerkey   &  Kasper Stoy
 *                      gerkey@usc.edu    kaspers@robotics.usc.edu
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
///////////////////////////////////////////////////////////////////////////
//
// Desc: Gazebo (simulator) fiducial driver
// Author: Andrew Howard
// Date: 9 Nov 2003
// CVS: $Id$
//
///////////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef INCLUDE_GAZEBO_POWER
#warning "gz_power not supported by libgazebo; skipping"
#else

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>       // for atoi(3)

#include "player.h"
#include "error.h"
#include "driver.h"
#include "drivertable.h"

#include "gazebo.h"
#include "gz_client.h"


// Incremental navigation driver
class GzPower : public Driver
{
  // Constructor
  public: GzPower(ConfigFile* cf, int section);

  // Destructor
  public: virtual ~GzPower();

  // Setup/shutdown routines.
  public: virtual int Setup();
  public: virtual int Shutdown();

  // Check for new data
  public: virtual void Update();
  
  // Commands
  public: virtual void PutCommand(player_device_id_t id,
                                  void* src, size_t len,
                                  struct timeval* timestamp);

  // Request/reply
  public: virtual int PutConfig(player_device_id_t id, void *client, 
                                void* src, size_t len,
                                struct timeval* timestamp);

  // Gazebo device id
  private: char *gz_id;

  // Gazebo client object
  private: gz_client_t *client;
  
  // Gazebo Interface
  private: gz_power_t *iface;

  // Timestamp on last data update
  private: double datatime;
};


// Initialization function
Driver* GzPower_Init(ConfigFile* cf, int section)
{
  if (GzClient::client == NULL)
  {
    PLAYER_ERROR("unable to instantiate Gazebo driver; did you forget the -g option?");
    return (NULL);
  }
  return ((Driver*) (new GzPower(cf, section)));
}


// a driver registration function
void GzPower_Register(DriverTable* table)
{
  table->AddDriver("gz_power", GzPower_Init);
  return;
}


////////////////////////////////////////////////////////////////////////////////
// Constructor
GzPower::GzPower(ConfigFile* cf, int section)
    : Driver(cf, section, PLAYER_POWER_CODE, PLAYER_ALL_MODE,
             sizeof(player_power_data_t), 0, 10, 10)
{
  // Get the globally defined  Gazebo client (one per instance of Player)
  this->client = GzClient::client;

  // Get the id of the device in Gazebo.
  // TODO: fix potential buffer overflow
  this->gz_id = (char*) calloc(1024, sizeof(char));
  strcat(this->gz_id, GzClient::prefix_id);
  strcat(this->gz_id, cf->ReadString(section, "gz_id", ""));

  // Create an interface
  this->iface = gz_power_alloc();

  this->datatime = -1;
    
  return;
}


////////////////////////////////////////////////////////////////////////////////
// Destructor
GzPower::~GzPower()
{
  gz_power_free(this->iface);
  
  return;
}


////////////////////////////////////////////////////////////////////////////////
// Set up the device (called by server thread).
int GzPower::Setup()
{ 
  // Open the interface
  if (gz_power_open(this->iface, this->client, this->gz_id) != 0)
    return -1;

  // Add ourselves to the update list
  GzClient::AddDriver(this);

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Shutdown the device (called by server thread).
int GzPower::Shutdown()
{
  // Remove ourselves to the update list
  GzClient::DelDriver(this);

  gz_power_close(this->iface);

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Check for new data
void GzPower::Update()
{
  player_power_data_t data;
  struct timeval ts;

  gz_power_lock(this->iface, 1);

  if (this->iface->data->time > this->datatime)
  {
    this->datatime = this->iface->data->time;

    ts.tv_sec = (int) (this->iface->data->time);
    ts.tv_usec = (int) (fmod(this->iface->data->time, 1) * 1e6);

    data.charge = htons((int16_t) (this->iface->data->levels[0] * 10));
    
    this->PutData(&data, sizeof(data), &ts);
  }

  gz_power_unlock(this->iface);

  return;
}


////////////////////////////////////////////////////////////////////////////////
// Commands
void GzPower::PutCommand(player_device_id_t id,
                         void* src, size_t len,
                         struct timeval* timestamp)
{  
  return;
}


////////////////////////////////////////////////////////////////////////////////
// Handle requests
int GzPower::PutConfig(player_device_id_t id, void *client, 
                       void* src, size_t len,
                       struct timeval* timestamp)
{
  uint8_t subtype;

  subtype = ((uint8_t*) src)[0];
  switch (subtype)
  {
    default:
    {
      if (PutReply(client, PLAYER_MSGTYPE_RESP_NACK,NULL) != 0)
        PLAYER_ERROR("PutReply() failed");
      break;
    }
  }
  return 0;
}

#endif