/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Support for the verb/device/modifier core logic and API,
 *  command line tool and file parser was kindly sponsored by
 *  Texas Instruments Inc.
 *  Support for multiple active modifiers and devices,
 *  transition sequences, multiple client access and user defined use
 *  cases was kindly sponsored by Wolfson Microelectronics PLC.
 *
 *  Copyright (C) 2021 Red Hat Inc.
 *  Authors: Jaroslav Kysela <perex@perex.cz>
 */

/**
 *  \defgroup ucm_conf Use Case Configuration
 *  The ALSA Use Case Configuration.
 *  See \ref Usecase_conf page for more details.
 *  \{
 */

/*! \page Usecase_conf ALSA Use Case Configuration

The use case configuration files use \ref conf syntax to define the
static configuration tree. This tree is evaluated (modified) at runtime
according the conditions and dynamic variables in the configuration tree.
The result is parsed and exported to the applications using \ref ucm API.

### Configuration directory and main filename lookup

The lookup paths are describen in *ucm.conf* file. The configuration
structure looks like:

~~~{.html}
UseCasePath.path1 {
  Directory "conf.virt.d"
  File "${OpenName}.conf"
}
UseCasePath.path2 {
  Directory "external"
  File "${OpenName}.conf"
}
~~~

### UCM main configuration file

Each sound card has a master sound card file that lists all the supported
use case verbs for that sound card. i.e.:

~~~{.html}
# Example master file for blah sound card
# By Joe Blogs <joe@bloggs.org>

Syntax 6

# Use Case name for user interface
Comment "Nice Abstracted Soundcard"

# The file is divided into Use case sections. One section per use case verb.

SectionUseCase."Voice Call" {
  File "voice_call_blah"
  Comment "Make a voice phone call."
}

SectionUseCase."HiFi" {
  File "hifi_blah"
  Comment "Play and record HiFi quality Music."
}

# Define Value defaults

ValueDefaults {
  PlaybackChannels 4
  CaptureChannels 4
}

# Define boot / initialization sequence
# This sequence is skipped, when the soundcard was already configured by system
# (alsactl configuration was already created). The purpose is to not alter
# ALSA card controls which may be modified by user after initial settings.

BootSequence [
  cset "name='Master Playback Switch',index=2 0,0"
  cset "name='Master Playback Volume',index=2 25,25"
  msleep 50
  cset "name='Master Playback Switch',index=2 1,1"
  cset "name='Master Playback Volume',index=2 50,50"
]

# Define fixed boot sequence
# This sequence is always executed on boot (hotplug).

FixedBootSequence [
  cset "name='Something to toggle' toggle"
]
~~~

### UCM verb configuration file

The verb configuration file defines devices, modifier and initialization sequences.
It is something like a sound profile.

~~~{.html}
# Example Use case verb section for Voice call blah
# By Joe Blogs <joe@blogs.com>

# verb global section

SectionVerb {

  # enable and disable sequences are compulsory
  EnableSequence [
    disdevall ""	# run DisableSequence for all devices
  ]

  DisableSequence [
    cset "name='Power Save' on"
  ]

  # Optional transition verb
  TransitionSequence."ToCaseName" [
    disdevall ""	# run DisableSequence for all devices
    msleep 1
  ]

  # Optional TQ and device values
  Value {
    TQ HiFi
    PlaybackChannels 6
  }
}

# Each device is described in new section. N devices are allowed

SectionDevice."Headphones" {

  SupportedDevice [
    "x"
    "y"
  ]

  # or (not both)

  ConflictingDevice [
    "x"
    "y"
  ]

  EnableSequence [
    ...
  ]

  DisableSequence [
    ...
  ]

  TransitionSequence."ToDevice" [
    ...
  ]

  Value {
    PlaybackVolume "name='Master Playback Volume',index=2"
    PlaybackSwitch "name='Master Playback Switch',index=2"
    PlaybackPCM "hw:${CardId},4"
  }
}

# Each modifier is described in new section. N modifiers are allowed

SectionModifier."Capture Voice" {
  Comment "Record voice call"

  SupportedDevice [
    "x"
    "y"
  ]

  # or (not both)

  ConflictingDevice [
    "x"
    "y"
  ]

  EnableSequence [
    ...
  ]

  DisableSequence [
    ...
  ]

  TransitionSequence."ToModifierName" [
    ...
  ]

  # Optional TQ and ALSA PCMs
  Value {
    TQ Voice
    CapturePCM "hw:${CardId},11"
    PlaybackMixerElem "Master"
    PlaybackVolume "name='Master Playback Volume',index=2"
    PlaybackSwitch "name='Master Playback Switch',index=2"
  }
}
~~~

### Sequence graphs

\image html ucm-seq-verb.svg
\image html ucm-seq-device.svg

### Sequence commands

Command name   | Description
---------------|----------------------------------------------
enadev2 ARG    | execute device enable sequence
disdev2 ARG    | execute device disable sequence
disdevall ""   | execute device disable sequence for all devices in verb
cdev ARG       | ALSA control device name for snd_ctl_open()
cset ARG       | ALSA control set - snd_ctl_ascii_elem_id_parse() + snd_ctl_ascii_value_parse()
cset-new ARG   | Create new ALSA user control element - snd_ctl_ascii_elem_id_parse() + description
ctl-remove ARG | Remove ALSA user control element - snd_ctl_ascii_elem_id_parse()
sysw ARG       | write to sysfs tree
usleep ARG     | sleep for specified amount of microseconds
msleep ARG     | sleep for specified amount of milliseconds
exec ARG       | execute a specific command (without shell - *man execv*)
shell ARG      | execute a specific command (using shell - *man system*)
cfg-save ARG   | save LibraryConfig to a file

~~~{.html}
# Examples
cset "name='PCM Playback Volue',index=2 99"
cset-new "name='Bool2' type=bool,count=2 1,0"
cset-new "name='Enum' type=enum,labels='L1;L2;L3' 'L2'"
ctl-remove "name='Bool2'"
sysw "-/class/sound/ctl-led/speaker/card${CardNumber}/attach:Speaker Channel Switch"
usleep 10
exec "/bin/echo hello"
shell "set"
cfg-save "/tmp/test.conf:+pcm"
~~~

### Naming (devices, verbs)

See the SND_USE_CASE_VERB constains like #SND_USE_CASE_VERB_HIFI for the full list of known verbs.

See the SND_USE_CASE_DEV constants like #SND_USE_CASE_DEV_SPEAKER for the full list of known devices.
If multiple devices with the same name exists, the number suffixes should
be added to these names like HDMI1,HDMI2,HDMI3 etc. No number gaps are
allowed. The names with numbers must be continuous. It is allowed to put
a whitespace between name and index (like 'Line 1') for the better
readability. The device names 'Line 1' and 'Line1' are equal for
this purpose.

If EnableSequence/DisableSequence controls independent paths in the hardware
it is also recommended to split playback and capture UCM devices and use
the number suffixes. Example use case: Use the integrated microphone
in the laptop instead the microphone in headphones.

The preference of the devices is determined by the priority value (higher value = higher priority).

See the SND_USE_CASE_MOD constants like #SND_USE_CASE_MOD_ECHO_REF for the full list of known modifiers.

### Boot (alsactl)

The *FixedBootSequence* is executed at each boot. The *BootSequence* is executed only
if the card's configuration is missing. The purpose is to let the users modify the
configuration like volumes or switches. The alsactl ensures the persistency (store
the state of the controls to the /var tree and loads the previous state in the next
boot).

\image html ucm-seq-boot.svg

### Device volume

It is expected that the applications handle the volume settings. It is not recommended
to set the fixed values for the volume settings in the Enable / Disable sequences for
verbs or devices, if the device exports the hardware volume (MixerElem or Volume/Switch
values). The default volume settings should be set in the *BootSequence*. The purpose
for this scheme is to allow users to override defaults using the alsactl sound card
state management.

Checklist:

1. Set default volume in BootSequence
2. Verb's EnableSequence should ensure that all devices are turned off (mixer paths)
   to avoid simultaneous device use - the previous state is unknown (see *disdevall*
   and *disdev2* commands or create a new custom command sequence)

\image html ucm-volume.svg

### Dynamic configuration tree

The evaluation order may look a bit different from the user perspective.
At first, the standard alsa-lib configuration tree is parsed. All other
layers on top are working with this tree. It may shift / move the configuration
blocks from the configuration files as they are placed to the tree internally.

~~~{.html}
Example configuration       | Parsed static tree      | Identical static tree
----------------------------+-------------------------+-------------------------------
If.1 {                      | If {                    | If.1.True.Define.VAR "A"
  True.Define.VAR "A"       |   1.True.Define.VAR "A" | If.2.True.Define.VAR "C"
}                           |   2.True.Define.VAR "C" | Define.VAR "B"
Define.VAR "B"              | }                       |
If.2 {                      | Define.VAR "B"          |
  True.Define.VAR "C"       |                         |
}                           |                         |
~~~

Even if one or both conditions are evaluated as true, the variable _VAR_ will
be evaluated always as **B** because the first _If_ block was before the non-nested
_Define_ . The second _If_ block was appended to the first _If_ block (before
_Define_ in the configuration tree) in the text configuration parser.


### Syntax

Unless described, the syntax version 4 is used.

~~~
Syntax 4
~~~


### Include

There are two ways to include other configuration files.

#### Static include

The static include is inherited from the standard alsa-lib configuration
syntax. It can be placed anywhere in the configuration files. The search
path is composed from the root alsa configuration path (usually
_/usr/share/alsa_) and _ucm2_ directory.

~~~{.html}
<some/path/file.conf>        # include file using the search path
</absolute/path/file.conf>   # include file using the absolute path
~~~

#### Lazy include

The lazy include is evaluated at runtime. The root path is the ucm2
tree. The absolute include appends the ucm2 absolute path to the
specified path. The relative include is relative to the file which
contains the _Include_ configuration block.

~~~{.html}
Include.id1.File "/some/path/file.conf"  # absolute include (in the ucm2 tree)
Include.id2.File "subdir/file.conf"      # relative include to the current configuration directory (UseCasePath)
~~~

### Configuration tree evaluation

The evaluation of the static configuration tree is proceed in
the specific order (see table bellow). When the dynamic configuration
tree changes, the evaluation sequence is restarted to evaluate
all possible changes (new *Define* or *Include* or *If* blocks).

Evaluation order   | Configuration block | Evaluation restart
------------------:|---------------------|--------------------
1                  | Define              | No
2                  | Include             | Yes
3                  | If                  | Yes


### Substitutions

The dynamic tree identifiers and assigned values in the configuration tree are
substituted. The substitutes strings are in the table bellow.

Substituted string   | Value
---------------------|---------------------
${OpenName}          | Original UCM card name (passed to snd_use_case_mgr_open())
${ConfLibDir}        | Library top-level configuration directory (e.g. /usr/share/alsa)
${ConfTopDir}        | Top-level UCM configuration directory (e.g. /usr/share/alsa/ucm2)
${ConfDir}           | Card's UCM configuration directory (e.g. /usr/share/alsa/ucm2/conf.d/USB-Audio)
${ConfName}          | Configuration name (e.g. USB-Audio.conf)
${CardNumber}        | Real ALSA card number (or empty string for the virtual UCM card)
${CardId}            | ALSA card identifier (see snd_ctl_card_info_get_id())
${CardDriver}        | ALSA card driver (see snd_ctl_card_info_get_driver())
${CardName}          | ALSA card name (see snd_ctl_card_info_get_name())
${CardLongName}      | ALSA card long name (see snd_ctl_card_info_get_longname())
${CardComponents}    | ALSA card components (see snd_ctl_card_info_get_components())
${env:<str>}         | Environment variable <str>
${sys:<str>}         | Contents of sysfs file <str>
${var:<str>}         | UCM parser variable (set using a _Define_ block)
${eval:<str>}        | Evaluate expression like *($var+2)/3* [**Syntax 5**]
${find-card:<str>}   | Find a card - see _Find card substitution_ section
${find-device:<str>} | Find a device - see _Find device substitution_ section

#### Special whole string substitution

Substituted string   | Value
---------------------|---------------------
${evali:<str>}       | Evaluate expression like *($var+2)/3* [**Syntax 6**]; target node will be integer; substituted only in the LibraryConfig subtree

#### Find card substitution

This substitutions finds the ALSA card and returns the appropriate identifier or
the card number (see return argument).

Usage example:

~~~{.html}
${find-card:field=name,regex='^acp$',return=number}
~~~

Arguments:

Argument             | Description
---------------------|-----------------------
return               | return value type (id, number), id is the default
field                | field for the lookup (id, driver, name, longname, mixername, components)
regex                | regex string for the field match

#### Find device substitution

Usage example:

~~~{.html}
${find-device:type=pcm,field=name,regex='DMIC'}
~~~

Arguments:

Argument             | Description
---------------------|-----------------------
type                 | device type (pcm)
stream               | stream type (playback, capture), playback is default
field                | field for the lookup (id, name, subname)
regex                | regex string for the field match


### Variable defines

The variables can be defined and altered with the *Define* or *DefineRegex* blocks.
The *Define* block looks like:

~~~{.html}
Define {
  variable1 "a"
  variable2 "b"
}
~~~

The *DefineRegex* allows substring extraction like:

~~~{.html}
DefineRegex.rval {
  Regex "(hello)|(regex)"
  String "hello, it's my regex"
}
~~~

The result will be stored to variables *rval1* as *hello* and *rval2* as *regex* (every matched
substrings are stored to a separate variable with the sequence number postfix.

Variables can be substituted using the `${var:rval1}` reference for example.

### Macros

Macros were added for *Syntax* version *6*. The *DefineMacro* defines new
macro like:

~~~{.html}
DefineMacro.macro1 {
  Define.a "${var:__arg1}"
  Define.b "${var:__other}"
  # Device or any other block may be defined here...
}
~~~

The arguments in the macro are refered as the variables with the double
underscore name prefix (like *__variable*). The configuration block in
the DefineMacro subtree is always evaluated (including arguments and variables)
at the time of the instantiation.

The macros can be instantiated (expanded) using:

~~~{.html}
# short version
Macro.id1.macro1 "arg1='something 1',other='other x'"

# long version
Macro.id1.macro1 {
  arg1 'something 1'
  other 'other x'
}
~~~

The second identifier (in example as *id1*) must be unique, but the contents
is ignored. It just differentiate the items in the subtree (allowing multiple
instances for one macro).

### Conditions

The configuration tree evaluation supports the conditions - *If* blocks. Each *If* blocks
must define a *Condition* block and *True* or *False* blocks or both. The *True* or *False*
blocks will be merged to the parent tree (where the *If* block is defined) when
the *Condition* is evaluated.

Example:

~~~{.html}
If.uniqueid {
  Condition {
    Type String
    Haystack "abcd"
    Needle "a"
  }
  True {
    Define.a a
    define.b b
  }
}
~~~

#### True (Type AlwaysTrue)

Execute only *True* block. It may be used to change the evaluation order as
explained in the *Configuration Tree* paragraph.

#### String is empty (Type String)

Field                | Description
---------------------|-----------------------
Empty                | string

#### Strings are equal (Type String)

Field                | Description
---------------------|-----------------------
String1              | string
String2              | substring in string

#### Substring is present (Type String)

Field                | Description
---------------------|-----------------------
Haystack             | string
Needle               | substring in string

#### Regex match (Type RegexMatch)

Field                | Description
---------------------|-----------------------
String               | string
Regex                | regex expression (extended posix, ignore case)

#### ALSA control element exists (Type ControlExists)

Field                | Description
---------------------|-----------------------
Device               | ALSA control device (see snd_ctl_open())
Control              | control in ASCII (parsed using snd_ctl_ascii_elem_id_parse())
ControlEnum	     | value for the enum control (optional)

Example:

~~~{.html}
If.fmic {
  Condition {
    Type ControlExists
    Control "name='Front Mic Playback Switch'"
  }
  True {
    ...
  }
}
~~~

### Variants

To avoid duplication of the many configuration files for the cases with
minimal configuration changes, there is the variant extension. Variants were
added for *Syntax* version *6*.

The bellow example will create two verbs - "HiFi" and "HiFi 7.1" with
the different playback channels (2 and 8) for the "Speaker" device.

Example (main configuration file):

~~~{.html}
SectionUseCase."HiFi" {
  File "HiFi.conf"
  Variant."HiFi" {
    Comment "HiFi"
  }
  Variant."HiFi 7+1" {
    Comment "HiFi 7.1"
  }
}
~~~

Example (verb configuration file - HiFi.conf):

~~~{.html}
SectionDevice."Speaker" {
  Value {
    PlaybackChannels 2
  }
  Variant."HiFi 7+1".Value {
    PlaybackChannels 8
  }
}
~~~

*/

/**
 \}
 */
