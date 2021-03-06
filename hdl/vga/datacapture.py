from migen.fhdl.std import *
from migen.genlib.cdc import MultiReg, PulseSynchronizer
from migen.genlib.fifo import AsyncFIFO, SyncFIFO
from migen.genlib.record import Record
from migen.bank.description import *
from migen.flow.actor import *


class DataCapture(Module, AutoCSR):
    def __init__(self, pads):
        '''
        self.clock_domains.cd_vgapix = ClockDomain()
        self.comb += [
          self.cd_vgapix.clk.eq(pads.datack),
          self.cd_vgapix.rst.eq(ResetSignal()) # XXX FIXME
        ]
        '''

        self.counterX = Signal(16)
        self.counterY = Signal(16)

        self.r = Signal(8)
        self.g = Signal(8)
        self.b = Signal(8)
        self.de = Signal()
        self.vsync = Signal()
        self.hsync = Signal()
        self.valid = Signal()

        hActive = Signal()
        vActive = Signal()

        vsout = Signal()
        self.comb += vsout.eq(pads.vsout)
        vsout_r = Signal()
        vsout_rising_edge = Signal()
        self.comb += vsout_rising_edge.eq(vsout & ~vsout_r)
        self.sync.pix += vsout_r.eq(vsout)

        hsout = Signal()
        self.comb += hsout.eq(pads.hsout)
        hsout_r = Signal()
        hsout_rising_edge = Signal()
        self.comb += hsout_rising_edge.eq(hsout & ~hsout_r)
        self.sync.pix += hsout_r.eq(hsout)

        r = Signal(8)
        g = Signal(8)
        b = Signal(8)

        # Interchange Red and Blue channels
        # instead of 0:8 we have to take 2:10 that is higher bits
        self.comb += [
            r.eq(pads.blue[2:10]),
            g.eq(pads.green[2:10]),
            b.eq(pads.red[2:10]),
            self.vsync.eq(vsout),
            self.hsync.eq(hsout),
        ]

        self.sync.pix += [

            self.r.eq(r),
            self.g.eq(g),
            self.b.eq(b),

            self.counterX.eq(self.counterX + 1),

            If(hsout_rising_edge,
                self.counterX.eq(0),
                self.counterY.eq(self.counterY + 1)
            ),

            If(vsout_rising_edge,
               self.counterY.eq(0),
            ),

            If((136+160 < self.counterX) & (self.counterX <= 136+160+1024),
                hActive.eq(1)
            ).Else(
                hActive.eq(0)
            ),

            If((6+29 < self.counterY) & (self.counterY <= 6+29+768),
                vActive.eq(1)
            ).Else(
                vActive.eq(0)
            ),
        ]

        #FIXME : valid signal should be proper
        self.comb += [
            self.valid.eq(1),
            self.de.eq(vActive & hActive),
        ]
