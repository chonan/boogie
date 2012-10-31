Linux tablet driver for Improv "Boogie board RIP"
=================================================

Install
-------

Compile kernel module 'boogie.ko' and install it.

    $ make
    $ sudo make install
    $ sudo rmmod usbhid
    $ sudo modprobe boogie
    $ sudo modprobe usbhid

Usage
-----

To use Improv Boogie board RIP as tablet, you needs to prepair/install
generic Linux input driver 'evdev'. On Debian/Ubuntu you have to check
whether 'xserver-xorg-input-evdev' package are installed on your system.

Then also check your xorg.conf if it contains 'InputClass' section
which have description using 'evdev' driver.

    Section "InputClass"
            Identifier "evdev tablet catchall"
            MatchIsTablet "on"
            MatchDevicePath "/dev/input/event*"
            Driver "evdev"
    EndSection

And restart xorg and enjoying.

Assignment
----------

* erase button - `BTN_STYLUS`
* save/wake button - `BTN_STYLUS2`
* touch led panel - `BTN_TOUCH`
* absolute coordinate - `ABS_X`, `ABS_Y`
* pressure ( 0 or 0xff ) - `ABS_PRESSURE`

Only magnetical stylus pen recognize. and driver do not care what
is drawn on led screen.

License
-------
Copyright &copy; 2012 Hiroshi Chonan <chonan@pid0.org>
Licensed under the [General Public License, Version 2.0][GPLv2]

[GPLv2]: http://www.gnu.org/licenses/gpl-2.0.html

ChangeLog
---------

## 0.1 (2012-09-12)

* Initial experimental release

## 0.2 (2012-10-31)

* Migrate to Kernel v3.5

