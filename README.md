# minColor

**minColor** is a free and open-source OCIO workflow plugin for Adobe After Effects. It manages your media and timeline color spaces — bypassing AE's "Interpret Media” workflow and using adjustment layers for viewport and render transforms.

> [!NOTE]
> minColor's approach is inspired by **Brendan Bolles' [fnordware OpenColorIO plug-in](https://www.fnord.com) for After Effects**. See [Credits & third-party notices](THIRD-PARTY-NOTICES.md).


---

## What it is:

minColor provides:

### A control panel for daily usage.

Easy batch actions for OCIO project management.

![mC_001.png](images/mC_001.png)

### And OCIO effects

Similar in usage to the Native AE OCIO effects, but not reliant on external configs.

![mC_011.png](images/mC_011.png)

---

## How to Install:

### macOS

Download `minColor-<version>.pkg` from [Releases](https://github.com/cbkow/minColor/releases/latest), quit After Effects, and run it. Signed and notarized.

### Windows

Download `minColor-<version>.msi` from [Releases](https://github.com/cbkow/minColor/releases/latest), quit After Effects, and run it. The installer is unsigned, so Windows will ask for permission to run it.

### Manually

Download `minColor-<version>.zip` from [Releases](https://github.com/cbkow/minColor/releases) and follow the included instructions to install both the plugin and the UI script.

---

## Usage:

The Effects layers work just like OCIO native plugins in AE but bypass AE’s color management. The Effects themselves handle OCIO configs internally — all of which are derived from ACES 2.0 and Blender 5.2 configs. They do not rely on OCIO configs from AE’s project settings.

1. First, you `migrate` a project (See Panel Controls Below). It loads a dummy OCIO profile into AE’s project settings that simply assigns the selected color space as both the media-default color space and the working color space. Meaning every item in an `ACEScg` project will, by default, be interpreted as `ACEScg` without additional transformations.

2. Set your viewing and rendering spaces in the panel and use the panel or the timeline to toggle between the View and Render Adjustment Layers when working/rendering.

3. Interpret Timeline as a batch process that will apply Rendering and View Adjustment Layers automatically and comb through an open timeline (and recursively all precomps within), then assign color profiles to each based on presets set in the `Matches…` button. (See Panel Controls Below)

> [!NOTE]
> This is a hack to effectively tell AE to ***stop thinking about what you are doing!*** The real OCIO profiles are embedded in the plugins and are not dependent on file paths.

**OCIO-based workflows (selected in Migration):**
- **ACEScg** is a combination of ACES 2.0 and Blender 5.2 configs. All transforms from both are included. The working and default media spaces are both **ACES AP1 (ACEScg)**, and all media transforms work from there.
- **ACES2065-1** is a combination of ACES 2.0 and Blender 5.2 configs. All transforms from both are included. The working and default media spaces are both **ACES AP0 (ACES 2065-1)**, and all media transforms work from that assumption.
- **Linear Rec.709** is a combination of ACES 2.0 and Blender 5.2 configs. All transforms from both are included. The working and default media spaces are both **Linear Rec.709 (Linear sRGB)**, and all media transforms work from that assumption. This has parity to Blender Default flows, including AGX.
- **Linear Rec.2020** is a combination of ACES 2.0 and Blender 5.2 configs. All transforms from both are included. The working and default media spaces are both **Linear Rec.2020**, and all media transforms assume that.
- **SDR** is meant to supplement Adobe's legacy ICC/ICM workflow for daily SDR work. The SDR config is based on a `Rec. 709, Gamma 2.2` working space, with easy transforms to `sRGB` and `Rec. 1886` for viewing and output.
  
> [!TIP]
> All profiles use the same working space and default media space, *so no more ACEScg vs ACES2065-1 dancing.*

Windows- and macOS-flavored view transforms are provided to counteract differences in how After Effects handles color management in each OS. Use this workaround for proper viewport colors on macOS until Adobe fixes it.

---

## Panel Controls

1. **Migrate a project** by selecting a working color space. If you pick ACEScg, the working space and default media space are ACEScg. 

![mC_001.png](images/mC_012.png)

2. **Interpret Timeline** walks the active comp and every precomp under it, matching all files to color spaces based on presets. 

![mC_009.png](images/mC_014.png)

3. Edit these presets with the **Matches** button. **Strip OCIO** removes all OCIO effects in the comp and all nested precomps recursively. It cleans up both minColor and native OCIO effects.

![mC_009.png](images/mC_013.png)

4. Use **Interpret Footage** to manually set the chosen layers' color spaces, or change any minColor effect's **Space** dropdown in Effect Controls to set it in place.

![mC_009.png](images/mC_015.png)

5. Set your **View** adjustment layer to what you want to see while working, and your **Render** adjustment layer to what you want to output. Use the buttons to quickly toggle between working and rendering.

![mC_001.png](images/mC_016.png)

---

## Healing:

The status line at the top of the panel is minColor's **Doctor**. It checks the project's OCIO pin whenever you click into the panel, press **Check**, or run a panel command. Green means the pin points at the project's own config; yellow means it drifted; red spells out anything to fix by hand.

The usual drift happens when you open a project on another machine or OS, and the stored dummy config path no longer resolves. Press **Repair** and minColor re-points the project to the `_minColor` config beside it, live, with no restart. One undo reverts it. Your renders are correct either way, because the effects carry their own configs; the pin only tells After Effects to stay out of the way.

A yellow "update available" means a newer config exists for your preset. Running **Migrate** again with the same preset refreshes it.

---

## The Extras:

To make things easier and reduce inconsistencies between AE on macOS and Windows, minColor provides common-name aliases for its View and Render options. Any **Desktop-labeled** View or Render option is an alias for `sRGB` flows—typical in web/social/direct/tech branding work. Any View or Render option labeled **"Video”** is better suited for broadcast delivery and standard commercial post-production workflows; they are aliases for `Rec. 709 gamma 2.4 / Rec. 1886 / BT. 1886`.

#### Web/Tech/Direct Motion Graphics
If you work for tech/web/direct, this will most likely match your normal workflow:
- **Windows Desktop View** or **macOS Desktop View** — depending on what machine you are currently sitting in front of.
- **Desktop Render**

#### Commercial and Broadcast
If you are working in post-production in an offline/online flow, you will most likely want:
- **Windows or macOS Desktop View** is what viewers will see on a TV or a professional broadcast monitor in an Online room.
- **Windows or macOS Video Views** are what people will see when they watch videos online or via a desktop video player. *Pick your poison (and lament the state of video today).*
- **Video Render** is what you will be outputting to offline editors and online finishing artists.

To be extra clear, except for the macOS flavors, these are all aliases for common settings like `sRGB` but are helpful for artists who are unfamiliar. The macOS views counteract a bug in how After Effects communicates with macOS. What is this bug?

> [!IMPORTANT]
> In OCIO projects, After Effects on macOS hands the viewer's pixels to the display without converting them to the monitor profile. Every other Mac app converts, and so does After Effects' own Adobe colour engine. On a Display P3 screen an sRGB or Rec.709 view is shown with P3 primaries and looks oversaturated. **The included macOS Views counteract this by encoding for the P3 panel directly.**

---

## Worth Noting:

#### ***How minColor sets your color management***

The AE SDK doesn't expose what you need to manage color space programmatically, so minColor sets the project's OCIO config for you through After Effects' own scripting bridge. **Migrate** writes a small `_minColor` folder next to your project that includes these OCIO configs.


> [!TIP]
> The minColor effect carries its own copy of every color transform, so it renders the same wherever the project lands — in the app and in `aerender` — with no config files to chase and no broken config paths.

## Credits:

Inspired by Brendan Bolles' fnordware OpenColorIO plug-in for After Effects. Built on
[OpenColorIO](https://opencolorio.org) (BSD-3-Clause) and color transforms from the Blender project
(AgX / Filmic, Troy Sobotka) and [ACES](https://acescentral.com) (A.M.P.A.S.). Full attributions and
licenses: [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
