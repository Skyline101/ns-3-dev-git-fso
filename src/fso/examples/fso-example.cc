/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 CTTC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Michael Di Perna <diperna.michael@gmail.com>
 */
#include "ns3/core-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/laser-antenna-model.h"
#include "ns3/optical-rx-antenna-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/fso-channel.h"
#include "ns3/fso-down-link-phy.h"
#include "ns3/fso-propagation-loss-model.h"
#include "ns3/fso-free-space-loss-model.h"
#include "ns3/fso-down-link-scintillation-index-model.h"
#include "ns3/fso-mean-irradiance-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FsoChannelExample");

// Sends a single packet from a geo-stationary satellite to an optical ground station
// A high evelation angle is assumed which corresponds to weak atmospheric turbulence
//
//                                        SATELLITE
//                                             |
//                                             |
//                                             |
//                                             |
//                                             V
//                                       GROUND STATION 
//
// Use verbose option to log output from the loss models and error model
//
// The RX/TX and link parameters can be found in the following papers: "Preliminary Results of Terabit-per-second Long-Range Free-Space Optical Transmission Experiment THRUST"
// and 
// "Overview of the Laser Communication System for the NICT Optical Ground Station and Laser Communication Experiments on Ground-to-Satellite Links"
//
// The channel uses 3 loss models:
// 
// 1) Free Space Path loss - the geometric power loss due to the propagation distance and wavelength
// 2) Scintillation Index  - A parameter which characterizes the fluctuations in irradiance due to atmospheric turbulence
// 3) Mean Irradiance      - The mean irradiance at the receiver due to the increase in the beamwidth as it propagates 
//
// The error model attached to the receiver computes the received irradiance based on the scintillation index and mean irradiance


int 
main (int argc, char *argv[])
{
  bool verbose = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);//!!!Need to use verbose argument

  cmd.Parse (argc,argv);

  LogComponentEnable ("FsoChannel", LOG_LEVEL_INFO);
  LogComponentEnable ("FsoDownLinkPhy", LOG_LEVEL_INFO);
  LogComponentEnable ("FsoFreeSpaceLossModel", LOG_LEVEL_INFO);
  LogComponentEnable ("FsoMeanIrradianceModel", LOG_LEVEL_INFO);
  LogComponentEnable ("FsoDownLinkErrorModel", LOG_LEVEL_INFO);

  //Mobility Models
  Ptr<MobilityModel> txMobility = CreateObject<ConstantPositionMobilityModel> ();
  txMobility->SetPosition (Vector (0.0, 0.0, 707000));//707,000 meters above the Earth

  Ptr<MobilityModel> rxMobility = CreateObject<ConstantPositionMobilityModel> ();
  rxMobility->SetPosition (Vector (0.0, 0.0, 0.0));// Ground station at 0 meters 

  //Antennas
  Ptr<LaserAntennaModel> laser = CreateObject<LaserAntennaModel> ();
  laser->SetBeamwidth (0.120); //meters
  laser->SetPhaseFrontRadius (707000); //meters - is approximately the link distance when r >> r0
  laser->SetOrientation (0.0);
  laser->SetTxPower (0.1);//Watts
  laser->SetGain (116.0);//dB

  Ptr<OpticalRxAntennaModel> receiver = CreateObject<OpticalRxAntennaModel> ();
  receiver->SetApertureDiameter (0.318);//meters
  receiver->SetRxGain (121.4);//dB
  receiver->SetOrientation (0.0);

  //Delay Model
  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();  

  //Propagation Loss Models
  Ptr<FsoFreeSpaceLossModel> freeSpaceLoss = CreateObject<FsoFreeSpaceLossModel> ();

  Ptr<FsoDownLinkScintillationIndexModel> scintIndexModel = CreateObject<FsoDownLinkScintillationIndexModel> ();
  scintIndexModel->SetRmsWindSpeed (21.0); //From the Hufnagel-Valley 5/7 model
  scintIndexModel->SetGndRefractiveIdx (1.7*10e-14); //From the Hufnagel-Valley 5/7 model
  
  Ptr<FsoMeanIrradianceModel> meanIrradianceModel = CreateObject<FsoMeanIrradianceModel> ();

  //Channel
  Ptr<FsoChannel> channel = CreateObject<FsoChannel> ();
  channel->SetPropagationDelayModel(delayModel);

  //Error Model
  Ptr<FsoDownLinkErrorModel> errorModel = CreateObject<FsoDownLinkErrorModel> ();

  //Phy
  double bitRate = 49.3724e6;
  Ptr<FsoDownLinkPhy> txPhy = CreateObject<FsoDownLinkPhy> ();
  txPhy->SetMobility (txMobility);
  txPhy->SetChannel (channel);
  txPhy->SetAntennas (laser, 0);
  txPhy->SetDevice (0);
  txPhy->SetBitRate (bitRate);
 
  Ptr<FsoDownLinkPhy> rxPhy = CreateObject<FsoDownLinkPhy> ();
  rxPhy->SetMobility (rxMobility);
  rxPhy->SetChannel (channel);
  rxPhy->SetAntennas (0, receiver);
  rxPhy->SetDevice (0);
  rxPhy->SetErrorModel (errorModel);
  rxPhy->SetBitRate (bitRate);
  errorModel->SetPhy (rxPhy);
  
  //Channel Setup
  channel->SetPropagationDelayModel (delayModel);
  channel->AddFsoPropagationLossModel (freeSpaceLoss);
  channel->AddFsoPropagationLossModel (scintIndexModel);
  channel->AddFsoPropagationLossModel (meanIrradianceModel);
  channel->Add(txPhy);
  channel->Add(rxPhy);

  //Setup Packet and Signal Params
  uint32_t size = 1024;//Packet size
  double wavelength = 847e-9;//nm 
  double speedOfLight = 3e8;//m/s
  Ptr<Packet> packet = Create<Packet> (size);

  FsoSignalParameters params;
  params.wavelength = wavelength;
  params.frequency = speedOfLight/wavelength;
  params.symbolPeriod = 1/bitRate;// 1/49.3724 Mbps
  params.power = 0.0;
  params.txPhy = txPhy;
  params.txAntenna = laser;
  params.txBeamwidth = 0.120/2.0;//meters - half the diameter
  params.txPhaseFrontRadius = 0.0;

  //Send packet from transmitter Phy
  txPhy->SendPacket (packet, params);
  

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


