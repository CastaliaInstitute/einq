# Einq vision (draft)

The X4 is a **room surface** — not an e-reader in our product. We are not building a reading experience (no EPUB library, no book UI). Through the day it shows calm, rotating **faces** chosen for the time of day and refreshed on a schedule.

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

**Einq firmware** — dedicated image for the X4: boots straight into the inq surface, WiFi for content sync and OTA, deep sleep between refreshes.

We used [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) early on as a known-good way to validate the board (display, USB flash, community SDK). That is **bring-up only**, not the product. New work targets **inq-face** on the open [X4 SDK](https://github.com/crosspoint-reader/community-sdk) stack without shipping reader features.

Implementation details (APIs, card JSON, offline cache, faculty RAG) come later. The clock spike proved display + schedule; **inq-face** is the product.
