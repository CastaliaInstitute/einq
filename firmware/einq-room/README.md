# Einq BLE room resolver

`EinqRoom::Resolver` converts configured BLE beacon observations into a stable
room selection. It is independent of NimBLE so it can be tested on a desktop
and fed by whichever scanner API the X3/X4 board layer uses.

```cpp
EinqRoom::Resolver rooms;
rooms.addBeacon("cafe0001-room-kitchen", "Kitchen");
rooms.addBeacon("cafe0002-room-studio", "Studio", -3);

rooms.observe(advertisedId, rssi, millis());
const EinqRoom::Result result = rooms.resolve(millis());
```

The default resolver requires three confident wins before changing rooms. A
result can remain unknown when signals are weak or too close to call.

`EinqRoomScanner` connects the resolver to NimBLE and the saved device
configuration. Beacon IDs may be:

- a lower-case BLE address such as `aa:bb:cc:dd:ee:ff`
- an advertised device name
- `ibeacon:<32 hex UUID digits>:<major>:<minor>`

The scanner uses rapid two-second windows while learning a room and changes to
a one-minute tracking cadence after it has a confident result.
