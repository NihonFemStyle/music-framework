# Discord Rich Presence

Nihon's Music Framework includes an optional, dependency-free Discord Rich Presence client for the normal Windows standalone build. Enable or disable it from the application's notification-area menu with **Discord Rich Presence**.

The default configuration uses Nihon's Music Framework application ID `1061319148602392616`. When nothing is playing, the activity uses the Discord application asset named `logo`. While music is playing it shows the track title, artist, album, playback state, and timestamps. Discord renders the timestamps as its native elapsed or remaining-time indicator.

## Using your own Discord application

1. Create an application in the [Discord Developer Portal](https://discord.com/developers/applications).
2. Open the application's Rich Presence assets page and upload a default image with a short asset key, such as `logo`.
3. Optionally upload an album-art or branding image with another asset key.
4. Start Nihon's Music Framework once so it creates `%LOCALAPPDATA%\NihonsMusicFramework\settings.ini`.
5. Exit the overlay, edit the `[DiscordRPC]` section, save the file, and restart it.

```ini
[DiscordRPC]
Enabled=1
ApplicationId=1061319148602392616
DefaultIconKey=logo
ArtworkKey=
```

- `Enabled` is `1` or `0`. The tray toggle updates this value.
- `ApplicationId` is the numeric application ID copied from Discord's **General Information** page.
- `DefaultIconKey` is the uploaded asset used while no music is active and as the music fallback.
- `ArtworkKey` is an optional uploaded asset used whenever music is active. Leave it empty to keep using `DefaultIconKey`.

## Album-art limitation

Windows GSMTC supplies album artwork as image bytes, while classic Discord Rich Presence IPC accepts an asset key registered to the selected Discord application. It cannot upload arbitrary cover bytes. Consequently, an `ArtworkKey` must refer to an image uploaded in the Discord Developer Portal. If it is blank, the configured default icon is used so the presence never depends on an unavailable asset.

## Activity links

Published activities include these buttons:

- **Trigon.Systems** — <https://trigon.systems>
- **GitHub Repo** — <https://github.com/NihonFemStyle/Music-Framework>

Discord must be running as the same Windows user. If Discord is closed, the overlay silently retries its local IPC connection; music detection and rendering continue normally.
