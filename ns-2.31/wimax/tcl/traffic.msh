# vim: syntax=tcl
# Copyright (C) 2007 Dip. Ing. dell'Informazione, University of Pisa, Italy
# http://info.iet.unipi.it/~cng/ns2mesh80216/
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA, USA
#

#
# 802.16 mesh Tcl scenario traffic models configuration
# author: C.Cicconetti <claudio.cicconetti@iet.unipi.it>
#

#
# load default values
#

set opt(trfsrc)   { }
set opt(trfdst)   { }

set opt(trftype-def)   "cbr"   ;# default traffic type
set opt(trfstart-def)   1.0     ;# default application start time
set opt(trfstop-def)   "never"  ;# default application start time
set opt(trfnsrc-def)    1       ;# default number of sources per flow
set opt(trfprio-def)    1       ;# default priority level
set opt(trftype)  { }
set opt(trfnsrc)  { }
set opt(trfstart) { }
set opt(trfstop)  { }
set opt(trfprio)  { }

set opt(voip-dur)    0    ;# average call duration (0 = no VoIP calls)
set opt(voip-start)  1    ;# start time of the first call
set opt(voip-prio)   1    ;# priority level of VoIP within the IEEE 802.16 mesh
set opt(voip-int)    60   ;# average interval between two consecutive calls
set opt(voip-model)  "one-to-one"   ;# VoIP VAD model
set opt(voip-codec)  "GSM.AMR"      ;# VoIP codec
set opt(voip-aggr)   2    ;# number of VoIP frames per packet

set opt(bwa-flows)   0        ;# number of random BWA traffic flows
set opt(bwa-rate)    100000   ;# rate of BWA traffic
set opt(bwa-pkt)     192      ;# packet size of BWA traffic flows, in bytes
set opt(bwa-start)   1        ;# start time of all BWA traffic flows
set opt(bwa-stop)    "never"  ;# end time of all BWA traffic flows
set opt(bwa-prio)    0        ;# priority of BWA traffic
set opt(bwa-fid)     5        ;# flow ID of BWA traffic as a whole

set opt(cbr-pkt)     1000     ;# packet size, in bytes
set opt(cbr-rate)    2000000  ;# rate, in b/s
set opt(cbr-rnd)     1        ;# set to 0 to have perfect CBR generation

set opt(vod-trace)   "traces/videoconf.mpeg4.ns2"

set opt(ftp-wnd)    64      ;# TCP maximum congestion window size
set opt(ftp-pkt)    1024    ;# TCP Maximum Segment Size

# can be one of
# - scan
# - sponsor
# - tunnel
# - link-est
set opt(net-entry)          ""
set opt(net-entry-node)     12
set opt(net-entry-sponsor)  11
set opt(net-entry-start)    1
set opt(net-entry-bs)       0

# 
# create a video traffic flow between two nodes, agents included
#
proc create_vod { src dst prio fid start stop } {
	global macmib opt ns node
	if { $opt(debug) != "" } {
		puts "create_vod $src $dst $prio $fid $start $stop"
	}

	set trace [new Tracefile]
	set app [new Application/Traffic/Trace]

	$trace filename $opt(vod-trace)
	$app attach-tracefile $trace

	set agtsrc [new Agent/UDP]
	set agtdst [new Agent/UDP]
	$agtsrc set class_ $fid
	$agtsrc set packetSize_  65536

	# set application start/stop times
	$ns at $start "$app start"
	if { $stop != "never" } {
		$ns at $stop "$app stop"
	}

	$macmib crc $fid crc
	$macmib priority $fid $prio
	$macmib precedence $fid 0

	$ns attach-agent $node($src) $agtsrc
	$ns attach-agent $node($dst) $agtdst
	$ns connect $agtsrc $agtdst
	$app attach-agent $agtsrc

	# end-to-end modules statistics collection
	set tag [new e2et]
	set mon [new e2em]
	$agtsrc attach-e2et $tag
	$agtdst attach-e2em $mon
	$mon index $fid
	$mon start-log
}
#
# create an UDP tunnel traffic flow between two nodes, agents included
#
proc create_udptunnel { src dst prio fid start stop } {
	global macmib opt ns node
	if { $opt(debug) != "" } {
		puts "create_udptunnel $src $dst $prio $fid $start $stop"
	}

	set agtsrc [new Agent/WimshUdpTunnel]
	set agtdst [new Agent/WimshUdpTunnel]
	if { $opt(debug) != "" } {
		$agtsrc set debug_ 1
		$agtdst set debug_ 1
	}
	$ns at $start "$agtsrc start"
	$ns stat add udp_tunnel_delay avg discrete

	$macmib crc $fid crc
	$macmib priority $fid $prio
	$macmib precedence $fid 0

	$agtsrc set class_ $fid
	$agtdst set class_ $fid

	$ns attach-agent $node($src) $agtsrc
	$ns attach-agent $node($dst) $agtdst
	$ns connect $agtsrc $agtdst

	# end-to-end modules statistics collection
	set tag [new e2et]
	set mon [new e2em]
	$agtsrc attach-e2et $tag
	$agtdst attach-e2em $mon
	$mon index $fid
	$mon start-log
}

#
# create an FTP traffic flow between two nodes, agents included
#
proc create_ftp { src dst prio fid start stop } {
	global macmib opt ns node
	if { $opt(debug) != "" } {
		puts "create_ftp $src $dst $prio $fid $start $stop"
	}

	set app [new Application/FTP]

	set agtsrc [new Agent/TCP/Reno]
	set agtdst [new Agent/TCPSink]	

	$agtsrc set class_ $fid
	$agtdst set class_ $fid
	$agtsrc set window_ $opt(ftp-wnd)
	$agtsrc set maxcwnd_ $opt(ftp-wnd)
	$agtsrc set packetSize_ $opt(ftp-pkt)
	$agtsrc set ts_resetRTO_ true

	$agtsrc tracevar cwnd_
	$agtsrc tracevar dupacks_
	$agtsrc tracevar ssthresh_
	$agtsrc tracevar rtt_
	$agtsrc tracevar srtt_

	# set application start/stop times
	$ns at $start "$app start"
	if { $stop != "never" } {
		$ns at $stop "$app stop"
	}

	$macmib crc $fid crc
	$macmib priority $fid $prio
	$macmib precedence $fid 0

	$ns attach-agent $node($src) $agtsrc
	$ns attach-agent $node($dst) $agtdst
	$ns connect $agtsrc $agtdst
	$app attach-agent $agtsrc

	# end-to-end modules statistics collection
	set tag [new e2et]
	set mon [new e2em]
	$agtsrc attach-e2et $tag
	$agtdst attach-e2em $mon
	$mon index $fid
	$mon start-log
	# $mon fairness-estimator $fairness
}
#
# create a CBR traffic flow between two nodes, agents included
#
proc create_cbr { src dst prio fid start stop } {
	global macmib opt ns node
	if { $opt(debug) != "" } {
		puts "create_cbr $src $dst $prio $fid $start $stop"
	}

	set app [new Application/Traffic/CBR]
	$app set random_ $opt(cbr-rnd)
	$app set packetSize_ $opt(cbr-pkt)
	$app set rate_ $opt(cbr-rate)

	set agtsrc [new Agent/UDP]
	set agtdst [new Agent/UDP]
	$agtsrc set class_ $fid

	# set application start/stop times
	$ns at $start "$app start"
	if { $stop != "never" } {
		$ns at $stop "$app stop"
	}

	$macmib crc $fid crc
	$macmib priority $fid $prio
	$macmib precedence $fid 0

	$ns attach-agent $node($src) $agtsrc
	$ns attach-agent $node($dst) $agtdst
	$ns connect $agtsrc $agtdst
	$app attach-agent $agtsrc

	# end-to-end modules statistics collection
	set tag [new e2et]
	set mon [new e2em]
	$agtsrc attach-e2et $tag
	$agtdst attach-e2em $mon
	$mon index $fid
	$mon start-log
	# $mon fairness-estimator $fairness
}
#
# create a BWA traffic flow between two nodes, agents included
#
proc create_bwa { src dst prio fid start stop } {
	global macmib opt ns node
	if { $opt(debug) != "" } {
		puts "create_bwa $src $dst $prio $fid $start $stop"
	}

	# create a RNG for this application
	set rng [new RNG]

	# create and configure the BWA Internet
	set app [new Application/Traffic/BwaInternet]
	$app pkt-size $opt(bwa-pkt)
	$app rate $opt(bwa-rate)
	$app use-rng $rng

	set agtsrc [new Agent/UDP]
	set agtdst [new Agent/UDP]
	$agtsrc set class_ $fid

	# set application start/stop times
	$ns at $start "$app start"
	if { $stop != "never" } {
		$ns at $stop "$app stop"
	}

	$macmib crc $fid crc
	$macmib priority $fid $prio
	$macmib precedence $fid 0

	$ns attach-agent $node($src) $agtsrc
	$ns attach-agent $node($dst) $agtdst
	$ns connect $agtsrc $agtdst
	$app attach-agent $agtsrc

	# end-to-end modules statistics collection
	set tag [new e2et]
	set mon [new e2em]
	$agtsrc attach-e2et $tag
	$agtdst attach-e2em $mon
	$mon index $fid
	$mon start-log
}

#
# create a VoIP traffic flow between two nodes, agents included
#
proc create_voip { src dst prio fid start stop } {
	global macmib opt ns node
	if { $opt(debug) != "" } {
		puts "create_voip $src $dst $prio $fid $start $stop"
	}

	# create a RNG for this application
	set rng [new RNG]

	# create and configure the VoIP application
	set app [new VoipSource]
	$app model $opt(voip-model)
	$ns at $start "$app start"
	if { $stop != "never" } {
		$ns at $stop "$app stop"
	}

	set encoder [new Application/VoipEncoder]
	$encoder codec $opt(voip-codec)
	$app encoder $encoder

	set decoder [new Application/VoipDecoderOptimal]
	$decoder id $fid
	$decoder cell-id 0
	$decoder emodel $opt(voip-codec)

	set aggregate [new Application/VoipAggregate]
	$aggregate nframes $opt(voip-aggr)

	set agtsrc [new Agent/UDP]
	set agtdst [new Agent/UDP]

	$agtsrc set class_ $fid

	$macmib crc $fid crc
	$macmib priority $fid $prio
	$macmib precedence $fid 0

	$ns attach-agent $node($src) $agtsrc
	$ns attach-agent $node($dst) $agtdst
	$ns connect $agtsrc $agtdst

	$encoder attach-agent $agtsrc
	$decoder attach-agent $agtdst

	$aggregate attach-agent $agtsrc
	$encoder aggregate $aggregate

	# end-to-end modules statistics collection
	set tag [new e2et]
	set mon [new e2em]
	$agtsrc attach-e2et $tag
	$agtdst attach-e2em $mon
	$mon index $fid
	$mon start-log
}
