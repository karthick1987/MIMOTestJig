%% LTE SIB1 Transmission over Two Antennas
% This example uses both channels of USRP(R) B210, X300 or X310 to transmit
% an LTE downlink signal that requires two antennas. The signal is generated
% by the LTE Toolbox(TM) and random bits are inserted into the SIB1 field,
% the first of the System Information Blocks. The accompanying example 
% <matlab:edit('sdruLTE2x2SIB1Rx.m') sdruLTE2x2SIB1Rx.m> receives this
% signal with two antennas, recovers the SIB1 data, and checks the CRC.
%
% This example uses the SDRu Transmitter System object(TM). The ChannelMapping
% property of the object is set to [1 2] to enable use of both channels.
% The step method takes a two-column matrix in which the first column is
% the signal for 'RF A' of the radio and the second column is the
% signal for 'RF B' of the radio.
%
% After starting this example, please run <matlab:edit('sdruLTE2x2SIB1Rx.m') sdruLTE2x2SIB1Rx.m>
% in a new MATLAB session. In Windows, if two B210 radios are used for these
% examples, each radio must be connected to a separate computer.
%
% Please refer to the Setup and Configuration section of <matlab:sdrudoc
% Documentation for USRP(R) Radio> for details on configuring your host
% computer to work with the SDRu Transmitter System object.
%
% Copyright 2015-2018 The MathWorks, Inc.

%% Generate LTE Signal

function [eNodeBOutput, Fs] = Generate_LTE_Frame_1TxAnt()

% Check for presence of LTE Toolbox
if isempty(ver('lte'))
    error(message('sdru:examples:NeedLST'));
end

% Generate LTE signal
rmc = lteRMCDL('R.9');      % Base RMC configuration
rmc.CellRefP = 1;           % 2 transmit antennas
rmc.NDLRB = 100;             % No. of Resource Blocks
rmc.PDSCH.NLayers = 1;      % 2 layers 
rmc.NCellID = 64;           % Cell identity
rmc.NFrame = 100;           % Initial frame number
rmc.TotSubframes = 2*10;      % Generate 2 frames. 10 subframes per frame
rmc.OCNGPDSCHEnable = 'On'; % Add noise to unallocated PDSCH resource elements
rmc.PDSCH.RNTI = 61;
rmc.SIB.Enable = 'On';
rmc.SIB.DCIFormat = 'Format1A';
rmc.SIB.AllocationType = 0;
rmc.SIB.VRBStart = 0;
rmc.SIB.VRBLength = 6;
rmc.SIB.Gap = 0;
rmc.SIB.Data = randi([0 1],144,1); % Use random bits in SIB data field. This is not a valid SIB message
trData = [1;0;0;1];
[eNodeBOutput,txGrid,rmc] = lteRMCDLTool(rmc,trData);

Fs = rmc.SamplingRate;
% NoOfSamples = size(eNodeBOutput,1);

% %% Plot Power Spectrum of Two-Channel LTE Signal
% 
% spectrumAnalyzer = dsp.SpectrumAnalyzer;
% spectrumAnalyzer.SampleRate = rmc.SamplingRate;  % 1.92e6 MHz for 'R.12'
% spectrumAnalyzer.Title = 'Power Spectrum of Two-Channel LTE Signal';
% spectrumAnalyzer.ShowLegend = true;
% spectrumAnalyzer.ChannelNames = {'Antenna 1', 'Antenna 2'};
% step(spectrumAnalyzer, eNodeBOutput);
% release(spectrumAnalyzer);

% save txWaveform.mat eNodeBOutput txGrid rmc;

% retval = writeBinary('LTESignal.bin',eNodeBOutput);
% 
% if(retval == 0)
%     sprintf("Writing to file sucessful\n");
% else
%     sprintf("Error: %d Writing to file failed\n",retval);
% end