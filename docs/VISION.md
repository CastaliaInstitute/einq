# Mynah eInq vision (draft)

Mynah X3 and Mynah X4 are Castalia room surfaces based on Xteink hardware.
They install CrossPoint, including its reader and system capabilities, while
booting into eInq as the default household experience. Through the day eInq
shows calm, rotating **faces** chosen for the time of day and refreshed on a
schedule.

## Primary experience

**Inq cards** — one card at a time, drawn from Castalia inquiry material:

- a **person** (faculty, figure, voice)
- a **place** (site, landscape, institution)
- a **thing** (concept, object, question)

Cards are readable at a glance on e-paper: title, a line or two, optional link or cue to go deeper elsewhere (Bibliotech, Castalia, etc.).

**Card art (v0):** no photos or custom bitmaps yet. Each card’s visual is a single **monochrome [Noto Emoji](https://github.com/googlefonts/noto-emoji)** glyph — one emoji standing in for the person, place, or thing. Rendered as 1-bit on the panel (black on paper). Picking which codepoint per card is TBD; type (person / place / thing) may default to a small fixed set until content drives specific glyphs.

Details of card schema, sourcing, and rotation TBD.

**Alongside or between cards** (schedule TBD):

- a **quote** (e.g. from [quotes.castalia.institute](https://quotes.castalia.institute))
- **mindfulness reminders** — short, non-pushy prompts aligned with household rhythm
- light **time context** (hour, part of day) when useful

## Scheduling (sketch)

The screen is a function of **day and time**. Rough bands, not fixed yet:

| Band | Example content |
|------|------------------|
| Morning | Inq card + intention |
| Day | Quote or card rotation |
| Evening | Reflection, softer reminder |
| Night | Minimal face or sleep |

Refresh policy: partial updates when only text changes; full refresh at day boundaries or on ghosting. WiFi sync only on a long interval or manual wake — not always-on.

## Firmware

**Mynah firmware** — CrossPoint plus the eInq application and shared Castalia
modules: boots straight into the eInq surface, keeps the CrossPoint home and
reader available, uses WiFi for content sync and OTA, and sleeps between
refreshes.

We use [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
as the installed base system for both Mynah models. New work targets
**inq-face** as a first-class CrossPoint application and should preserve
reader, system, and recovery paths.

Implementation details (APIs, card JSON, offline cache, faculty RAG) come later. The clock spike proved display + schedule; **inq-face** is the product.
