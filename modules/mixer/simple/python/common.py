#!/usr/bin/python
#  -*- coding: utf-8 -*-
#  -*- Python -*-

from pyalsa.alsahcontrol import HControl, Element as HElement, \
                                Info as HInfo, Value as HValue, InterfaceId, \
                                EventMask, EventMaskRemove

MIXER = InterfaceId['Mixer']
MIXERS = str(MIXER)

class BaseElement(InternalMElement):

  def __init__(self, mixer, name, index, weight):
    InternalMElement.__init__(self, mixer, name, index, weight)
    self.channels = 0
    self.min = [0, 0]
    self.max = [0, 0]

  def opsIsChannel(self, dir, chn):
    return chn >= 0 and chn < self.channels

  def opsGetRange(self, dir):
    return (0, self.min[dir], self.max[dir])

  def opsSetRange(self, dir, min, max):
    self.min[dir] = min
    self.max[dir] = max

  def volumeToUser(self, info, dir, value):
    min = info.min
    max = info.max
    if min == max:
      return self.min[dir]
    n = (value - min) * (self.max[dir] - self.min[dir])
    return self.min[dir] + (n + (max - min) / 2) / (max - min)

  def volumeFromUser(self, info, dir, value):
    min = info.min
    max = info.max
    if self.max[dir] == self.min[dir]:
      return min
    n = (value - self.min[dir]) * (max - min)
    return min + (n + (self.max[dir] - self.min[dir]) / 2) / (self.max[dir] - self.min[dir])

class StandardElement(BaseElement):

  def __init__(self, mixer, name, index, weight):
    BaseElement.__init__(self, mixer, name, index, weight)
    self.channels = 1
    self.volume = [None, None]
    self.volumeinfo = [None, None]
    self.volumetuple = [None, None]
    self.switch = [None, None]
    self.switchinfo = [None, None]
    self.switchtuple = [None, None]

  def decideChannels(self):
    max = 0
    for i in [0, 1]:
      if self.volume[i]:
        count = self.volumeinfo[i].count
        if count > max:
          max = count
      if self.switch[i]:
        count = self.switchinfo[i].count
        if count > max:
          max = count
    self.channels = max

  def attachVolume(self, helem, dir):
    self.volume[dir] = helem
    self.volumeinfo[dir] = HInfo(helem)
    self.min[dir] = self.volumeinfo[dir].min
    self.max[dir] = self.volumeinfo[dir].max
    self.volumetuple[dir] = HValue(helem).getTuple(self.volumeinfo[dir].type, self.volumeinfo[dir].count)

  def attachSwitch(self, helem, dir):
    self.switch[dir] = helem
    self.switchinfo[dir] = HInfo(helem)
    self.switchtuple[dir] = HValue(helem).getTuple(self.switchinfo[dir].type, self.switchinfo[dir].count)

  def attach(self, helem):
    BaseElement.attach(self, helem)
    if helem.name.endswith('Playback Volume'):
      self.attachVolume(helem, 0)
      self.caps |= self.CAP_PVOLUME
    elif helem.name.endswith('Capture Volume'):
      self.attachVolume(helem, 1)
      self.caps |= self.CAP_CVOLUME
    elif helem.name.endswith('Playback Switch'):
      self.attachSwitch(helem, 0)
      self.caps |= self.CAP_PSWITCH
    elif helem.name.endswith('Capture Switch'):
      self.attachSwitch(helem, 1)
      self.caps |= self.CAP_CSWITCH
    self.decideChannels()
    self.eventInfo()

  def opsGetVolume(self, dir, chn):
    return (0, self.volumeToUser(self.volumeinfo[dir], dir, self.volumetuple[dir][chn]))

  def opsSetVolume(self, dir, chn, value):
    val = self.volumeFromUser(self.volumeinfo[dir], dir, value)
    if self.volumetuple[dir][chn] == val:
      return
    a = list(self.volumetuple[dir])
    a[chn] = val
    self.volumetuple[dir] = tuple(a)
    hv = HValue(self.volume)
    hv.setTuple(self.volumeinfo[dir].type, self.volumetuple[dir])
    hv.write()

  def opsGetSwitch(self, dir, chn):
    return (0, self.switchtuple[dir][chn])

  def opsSetSwitch(self, dir, chn, value):
    if self.switchtuple[dir][chn] and value:
      return
    if not self.switchtuple[dir][chn] and not value:
      return
    a = list(self.switchtuple[dir])
    a[chn] = int(value)
    self.switchtuple[dir] = tuple(a)
    hv = HValue(self.switch[dir])
    hv.setTuple(self.switchinfo[dir].type, self.switchtuple[dir])
    hv.write()

  def update(self, helem):
    for i in [0, 1]:
      if helem == self.volume[i]:
        self.volumetuple[i] = HValue(helem).getTuple(self.volumeinfo[i].type, self.volumeinfo[i].count)
      elif helem == self.switch[i]:
        self.switchtuple[i] = HValue(helem).getTuple(self.switchinfo[i].type, self.switchinfo[i].count)
    self.eventValue()

class EnumElement(BaseElement):

  def __init__(self, mixer, name, index, weight):
    BaseElement.__init__(self, mixer, name, index, weight)
    self.mycaps = 0
  
  def attach(self, helem):
    BaseElement.attach(self, helem)
    self.enum = helem
    self.enuminfo = HInfo(helem)
    self.enumtuple = HValue(helem).getTuple(self.enuminfo.type, self.enuminfo.count)
    self.channels = self.enuminfo.count
    self.texts = self.enuminfo.itemNames
    self.caps |= self.mycaps

  def opsIsEnumerated(self, dir=-1):
    if dir < 0:
      return 1
    if dir == 0 and self.mycaps & self.CAP_PENUM:
      return 1
    if dir == 1 and self.mycaps & self.CAP_CENUM:
      return 1

  def opsIsEnumCnt(self, dir):
    return self.enuminfo.items

  def opsGetEnumItemName(self, item):
    return (0, self.texts[item])

  def opsGetEnumItem(self, chn):
    if chn >= self.channels:
      return -1
    return (0, self.enumtuple[chn])
    
  def opsSetEnumItem(self, chn, value):
    if chn >= self.channels:
      return -1
    if self.enumtuple[chn] == value:
      return
    a = list(self.enumtuple)
    a[chn] = int(value)
    self.enumtuple = tuple(a)
    hv = HValue(self.enum)
    hv.setTuple(self.enuminfo.type, self.enumtuple)
    hv.write()

  def update(self, helem):
    self.enumtuple = HValue(helem).getTuple(self.enuminfo.type, self.enuminfo.count)
    self.eventValue()

class EnumElementPlayback(EnumElement):

  def __init__(self, mixer, name, index, weight):
    EnumElement.__init__(self, mixer, name, index, weight)
    self.mycaps = self.CAP_PENUM
  
class EnumElementCapture(EnumElement):

  def __init__(self, mixer, name, index, weight):
    EnumElement.__init__(self, mixer, name, index, weight)
    self.mycaps = self.CAP_CENUM
  
ELEMS = []

def element_add(helem):
  key = helem.name+'//'+str(helem.index)+'//'+str(helem.interface)
  if not CONTROLS.has_key(key):
    return
  val = CONTROLS[key]
  felem = None
  for elem in ELEMS:
    if elem.name == val[0] and elem.index == val[1]:
      felem = elem
      break
  if not felem:
    felem = mixer.newMElement(val[3], val[0], val[1], val[2])
    mixer.addMElement(felem)
    ELEMS.append(felem)
  felem.attach(helem)

def eventHandler(evmask, helem, melem):
  if evmask == EventMaskRemove:
    return
  if evmask & EventMask['Add']:
    element_add(helem)
  if evmask & EventMask['Value']:
    melem.update(helem)

def init():
  hctl = HControl(device, load=False)
  mixer.attachHCtl(hctl)
  mixer.register()
