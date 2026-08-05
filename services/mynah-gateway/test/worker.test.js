import assert from "node:assert/strict";
import test from "node:test";

import { createGateway } from "../src/worker.js";

const session = {
  profile: "kid",
  ageBand: "tween",
  calendarEntity: "calendar.family",
  weatherEntity: "weather.home",
  calendarTitleMode: "busy",
  permissions: {
    calendar: true,
    astrology: true,
    fortune: true,
    cards: true,
    spotifyControl: true,
    lightControl: true,
    codexControl: true,
    administration: true
  },
  rooms: {
    Kitchen: {
      spotifyEntity: "media_player.kitchen",
      lightEntities: ["light.kitchen_1", "light.kitchen_2"],
      scene: "Evening"
    }
  }
};

const env = {
  DEVICE_SESSIONS: JSON.stringify({ secret: session }),
  HOME_ASSISTANT_URL: "https://ha.example",
  HOME_ASSISTANT_TOKEN: "ha-token",
  CASTALIA_CONTENT_URL: "https://castalia.example/home-content",
  CASTALIA_SERVICE_TOKEN: "content-token",
  SUPABASE_URL: "https://supabase.example",
  SUPABASE_ANON_KEY: "anon-key"
};

function response(value, status = 200) {
  return new Response(JSON.stringify(value), {
    status,
    headers: { "content-type": "application/json" }
  });
}

function request(path, options = {}) {
  return new Request(`https://mynah.example${path}`, {
    ...options,
    headers: { authorization: "Bearer secret", "content-type": "application/json", ...options.headers }
  });
}

test("health check requires no device token and exposes no household policy", async () => {
  const gateway = createGateway({ fetchImpl: async () => assert.fail("health must not call upstream services") });
  const result = await gateway.fetch(new Request("https://mynah.example/api/v1/health"), {});
  assert.equal(result.status, 200);
  assert.deepEqual(await result.json(), { ok: true, service: "mynah-gateway", apiVersion: 1 });
});

test("home payload aggregates content and HA state while redacting kid calendar titles", async () => {
  const calls = [];
  const fetchImpl = async (input, options = {}) => {
    const url = String(input);
    calls.push({ url, options });
    if (url.startsWith("https://castalia.example")) {
      return response({
        astrology: { title: "Moon", summary: "Look up." },
        aphorism: { title: "Mercury listens", summary: "Ask before deciding." },
        synastryWeather: { title: "Soft aspects", summary: "Make room for repair." },
        family: [{ name: "Camille", status: "Needs a quiet evening." }],
        fortune: { title: "Lantern", summary: "Carry light." },
        card: { title: "Threshold", summary: "What changes?", domain: "place" },
        news: { title: "Institute opens", summary: "A new seminar begins.", source: "news.castalia.institute" },
        artOfTheDay: { title: "Water Lilies", artist: "Claude Monet", image: "https://castalia.example/art.bmp" },
        quoteOfTheDay: { text: "Know thyself.", author: "Delphi" },
        mindfulness: { title: "Three breaths", summary: "Notice each exhale." },
        tasks: [{ title: "Return a library book", due: "17:00", completed: false }],
        library: {
          catalogUrl: "https://castalia.example/device/library/catalog",
          revision: "abc123",
          bookCount: 12,
          changedCount: 2
        },
        codex: {
          revision: 7,
          selectedIndex: 0,
          tasks: [{ id: "thread-1", title: "Build the Codex face", status: "running" }]
        }
      });
    }
    if (url.includes("/api/calendars/")) {
      return response([
        {
          summary: "Private therapy appointment",
          start: { dateTime: "2026-07-28T17:00:00-06:00" },
          end: { dateTime: "2026-07-28T18:00:00-06:00" }
        }
      ]);
    }
    if (url.endsWith("media_player.kitchen")) {
      return response({
        state: "playing",
        attributes: { media_title: "An Ending", media_artist: "Brian Eno", friendly_name: "Kitchen", volume_level: 0.34 }
      });
    }
    if (url.endsWith("weather.home")) {
      return response({ state: "partlycloudy", attributes: { temperature: 72, temperature_unit: "°F" } });
    }
    if (url.endsWith("light.kitchen_1")) return response({ state: "on", attributes: { brightness: 128 } });
    if (url.endsWith("light.kitchen_2")) return response({ state: "off", attributes: {} });
    throw new Error(`unexpected ${url}`);
  };
  const gateway = createGateway({ fetchImpl, now: () => new Date("2026-07-28T22:00:00Z") });
  const result = await gateway.fetch(request("/api/v1/device/home?room=Kitchen"), env);
  const body = await result.json();

  assert.equal(result.status, 200);
  assert.equal(body.profile, "kid");
  assert.equal(body.today.nextEvent.title, "Busy");
  assert.equal(body.weather.temperature, "72°F");
  assert.equal(body.day.aphorism.title, "Mercury listens");
  assert.equal(body.synastryWeather.title, "Soft aspects");
  assert.equal(body.family[0].name, "Camille");
  assert.equal(body.spotify.track, "An Ending");
  assert.equal(body.lights.on, true);
  assert.equal(body.lights.brightness, 50);
  assert.equal(body.schema, "castalia.device.daily.v1");
  assert.equal(body.date, "2026-07-28");
  assert.equal(body.news.title, "Institute opens");
  assert.equal(body.art.title, "Water Lilies");
  assert.equal(body.quote.author, "Delphi");
  assert.equal(body.mindfulness.title, "Three breaths");
  assert.equal(body.today.tasks[0].title, "Return a library book");
  assert.equal(body.library.bookCount, 12);
  assert.equal(body.codex.tasks[0].id, "thread-1");
  assert.equal(body.permissions.administration, false);
  assert.ok(calls.some((call) => {
    if (!call.url.startsWith("https://castalia.example")) return false;
    return JSON.parse(call.options.body).room === "Kitchen";
  }));
});

test("daily endpoint returns the same authenticated bundle contract", async () => {
  const dailyEnv = {
    ...env,
    DEVICE_SESSIONS: JSON.stringify({
      secret: { profile: "parent", permissions: {}, rooms: {} }
    })
  };
  const gateway = createGateway({
    fetchImpl: async (input) => {
      if (String(input).startsWith("https://castalia.example")) {
        return response({ quote: { text: "Begin.", author: "Castalia" } });
      }
      throw new Error(`unexpected ${input}`);
    },
    now: () => new Date("2026-07-29T06:00:00Z")
  });
  const result = await gateway.fetch(request("/api/v1/device/daily"), dailyEnv);
  const body = await result.json();
  assert.equal(result.status, 200);
  assert.equal(body.schema, "castalia.device.daily.v1");
  assert.equal(body.date, "2026-07-29");
  assert.equal(body.quote.text, "Begin.");
});

test("Scriptorium reads the authenticated individual's private EPUB catalog without exposing GitHub credentials", async () => {
  const calls = [];
  const scriptoriumEnv = {
    ...env,
    GITHUB_LIBRARY_TOKEN: "private-github-token",
    DEVICE_SESSIONS: JSON.stringify({
      secret: { individual: "dcmcshan", profile: "parent", permissions: {}, rooms: {} }
    })
  };
  const gateway = createGateway({
    fetchImpl: async (input, options = {}) => {
      const url = String(input);
      calls.push({ url, options });
      if (url.startsWith("https://api.github.com/repos/CastaliaInstitute/castalia-dcmcshan/")) {
        return response({
          schema: "castalia.individual.library.v1",
          individual: "dcmcshan",
          repository: "CastaliaInstitute/castalia-dcmcshan",
          format: "epub",
          books: [
            {
              id: "little-prince",
              title: "The Little Prince",
              authors: ["Antoine de Saint-Exupery"],
              path: "library/books/the-little-prince.epub",
              sha256: "abc"
            },
            { id: "unsafe", title: "Ignore", authors: [], path: "private/notes.txt" }
          ]
        });
      }
      if (url.startsWith("https://castalia.example")) return response({});
      throw new Error(`unexpected ${url}`);
    }
  });
  const result = await gateway.fetch(request("/api/v1/device/daily?segment=scriptorium"), scriptoriumEnv);
  const text = await result.text();
  const body = JSON.parse(text);

  assert.equal(result.status, 200);
  assert.ok(Buffer.byteLength(text) < 1700, "Scriptorium must fit the X3 TLS receive window");
  assert.equal(body.library.repository, "CastaliaInstitute/castalia-dcmcshan");
  assert.equal(body.library.bookCount, 2);
  assert.equal(body.library.books.length, 1);
  assert.equal(body.library.books[0].title, "The Little Prince");
  const githubCall = calls.find((call) => call.url.startsWith("https://api.github.com/"));
  assert.equal(githubCall.options.headers.authorization, "Bearer private-github-token");
  assert.equal(text.includes("private-github-token"), false);
});

test("Gazetteer daily sections are normalized into device-sized X of the Day entries", async () => {
  const gateway = createGateway({
    fetchImpl: async (input) => {
      if (!String(input).startsWith("https://castalia.example")) throw new Error(`unexpected ${input}`);
      return response({
        date: "2026-08-01",
        season: "summer",
        theme: { message: "Open through long light today." },
        content: {
          book: { title: "Walden", creator: "Henry David Thoreau", why: "Attend closely to place." },
          quote: { text: "I am rooted, but I flow.", attribution: "Virginia Woolf" },
          poem: { title: "Song of Myself", creator: "Walt Whitman", excerpt: "I celebrate myself.", note: "Notice the living world." },
          faculty: { name: "Attention", practice: "Notice one ordinary thing.", astrology_note: "Practice warmth." },
          history: { title: "Apollo 11 reaches the Moon", date: "1969-07-20", note: "Patient preparation matters." },
          country: { name: "Japan", capital: "Tokyo", reflection: "Carry careful craft into the day." },
          art: { title: "The Great Wave", artist: "Katsushika Hokusai", medium: "woodblock print", looking_prompt: "Look for force and craft." },
          bible: { reference: "Micah 6:8", text: "Do justice, love kindness.", reflection: "Make kindness concrete." }
        }
      });
    },
    now: () => new Date("2026-08-01T12:00:00Z")
  });
  const result = await gateway.fetch(request("/api/v1/device/daily"), env);
  const body = await result.json();
  assert.equal(result.status, 200);
  assert.equal(body.gazetteer.book.title, "Walden");
  assert.equal(body.gazetteer.poem.byline, "Walt Whitman");
  assert.equal(body.gazetteer.faculty.title, "Attention");
  assert.equal(body.gazetteer.history.byline, "1969-07-20");
  assert.equal(body.gazetteer.country.title, "Japan");
  assert.equal(body.gazetteer.bible.title, "Micah 6:8");
  assert.equal(body.art.title, "The Great Wave");
  assert.equal(body.quote.byline, "Virginia Woolf");
});

test("long Gazetteer excerpts are split into TLS-safe device segments", async () => {
  const longText = "A".repeat(400);
  const segmentEnv = {
    ...env,
    DEVICE_SESSIONS: JSON.stringify({ secret: { profile: "parent", permissions: {}, rooms: {} } })
  };
  const gateway = createGateway({
    fetchImpl: async (input) => {
      if (!String(input).startsWith("https://castalia.example")) throw new Error(`unexpected ${input}`);
      return response({
        selfWeather: { title: "Virgo Moon", summary: longText.repeat(2) },
        synastryWeather: { title: "Balanced household", summary: longText.repeat(2) },
        season: "summer",
        theme: { message: "Long light." },
        content: {
          book: { title: "Book", why: longText },
          art: { title: "Art", looking_prompt: longText },
          quote: { text: longText },
          poem: { title: "Poem", excerpt: longText },
          faculty: { name: "Attention", practice: longText },
          history: { title: "History", note: longText },
          country: { name: "Country", reflection: longText },
          bible: { reference: "Verse", text: longText }
        }
      });
    }
  });

  const segments = {};
  for (const name of [
    "core",
    "astrology-self",
    "astrology-synastry",
    "scriptorium",
    "daily-1",
    "daily-2",
    "daily-3",
    "daily-4"
  ]) {
    const result = await gateway.fetch(request(`/api/v1/device/daily?segment=${name}`), segmentEnv);
    const text = await result.text();
    assert.equal(result.status, 200);
    assert.ok(Buffer.byteLength(text) < 1700, `${name} must fit the X3 TLS receive window`);
    segments[name] = JSON.parse(text);
  }
  assert.equal(segments.core.selfWeather, undefined);
  assert.equal(segments.core.synastryWeather, undefined);
  assert.equal(segments["astrology-self"].selfWeather.title, "Virgo Moon");
  assert.equal(segments["astrology-synastry"].synastryWeather.title, "Balanced household");
  assert.equal(segments["daily-1"].gazetteer.book.title, "Book");
  assert.equal(segments["daily-2"].quote.title, "Quote of the Day");
  assert.equal(segments["daily-3"].gazetteer.faculty.title, "Attention");
  assert.equal(segments["daily-4"].gazetteer.country.title, "Country");
});

test("private family Gazetteer astrology overlays the generic content fallback", async () => {
  const familyEnv = {
    ...env,
    DEVICE_SESSIONS: JSON.stringify({
      secret: {
        profile: "parent",
        individual: "dcmcshan",
        familyRepository: "CastaliaInstitute/castalia-family-mcshan",
        permissions: { astrology: true },
        rooms: {}
      }
    }),
    FAMILY_RHYTHM_GITHUB_TOKEN: "github-read-token"
  };
  const calls = [];
  const gateway = createGateway({
    now: () => new Date("2026-08-05T18:00:00Z"),
    fetchImpl: async (input, options = {}) => {
      const url = String(input);
      calls.push({ url, options });
      if (url.startsWith("https://castalia.example")) {
        return response({ selfWeather: { title: "Fallback", summary: "Not calculated." } });
      }
      if (url.includes("CastaliaInstitute/castalia-family-mcshan/contents/outputs/2026-08-05/daily-content.json")) {
        return response({
          selfWeather: { title: "Sun square Sun", summary: "Calculated transit reading." },
          synastryWeather: { title: "Household synastry", summary: "Calculated pair reading." },
          astrologyMeta: {
            source: "ephemeris.castalia.institute · Swiss Ephemeris 2.10.03",
            effectiveAt: "2026-08-05T18:00:00+00:00"
          }
        });
      }
      throw new Error(`unexpected ${url}`);
    }
  });

  const result = await gateway.fetch(request("/api/v1/device/daily?segment=astrology-self"), familyEnv);
  const body = await result.json();
  assert.equal(result.status, 200);
  assert.equal(body.selfWeather.title, "Sun square Sun");
  assert.match(body.astrologyMeta.source, /ephemeris\.castalia\.institute/);
  const githubCall = calls.find((call) => call.url.startsWith("https://api.github.com/"));
  assert.equal(githubCall.options.headers.authorization, "Bearer github-read-token");
});

test("configured calendar IDs resolve only through the household allow-list", async () => {
  const householdCalendar =
    "ca7a560e76044c59bbb72a70b98a21a774b99c2f5195eb7357ecd1a1cdf74344@group.calendar.google.com";
  const calendarSession = {
    ...session,
    calendars: { [householdCalendar]: "calendar.daniel_camille" },
    defaultCalendarId: householdCalendar
  };
  const calendarEnv = {
    ...env,
    DEVICE_SESSIONS: JSON.stringify({ secret: calendarSession })
  };
  const calls = [];
  const fetchImpl = async (input) => {
    const url = String(input);
    calls.push(url);
    if (url.startsWith("https://castalia.example")) return response({});
    if (url.includes("calendar.daniel_camille")) {
      return response([{ summary: "Family dinner", start: { dateTime: "2026-07-29T00:00:00Z" } }]);
    }
    if (url.endsWith("media_player.kitchen")) return response({ state: "idle", attributes: {} });
    if (url.endsWith("weather.home")) return response({ state: "sunny", attributes: { temperature: 70 } });
    if (url.includes("light.kitchen")) return response({ state: "off", attributes: {} });
    throw new Error(`unexpected ${url}`);
  };
  const gateway = createGateway({ fetchImpl, now: () => new Date("2026-07-28T22:00:00Z") });

  const allowed = await gateway.fetch(
    request(`/api/v1/device/home?room=Kitchen&calendar=${encodeURIComponent(householdCalendar)}`),
    calendarEnv
  );
  assert.equal(allowed.status, 200);
  assert.equal((await allowed.json()).today.nextEvent.title, "Busy");
  assert.ok(calls.some((url) => url.includes("calendar.daniel_camille")));

  calls.length = 0;
  const rejected = await gateway.fetch(
    request("/api/v1/device/home?room=Kitchen&calendar=attacker%40example.com"),
    calendarEnv
  );
  assert.equal(rejected.status, 200);
  assert.equal((await rejected.json()).today.nextEvent, null);
  assert.ok(calls.every((url) => !url.includes("/api/calendars/")));
});

test("disabled content categories are omitted from the device payload", async () => {
  const restrictedSession = {
    ...session,
    permissions: { ...session.permissions, astrology: false, fortune: false, cards: false }
  };
  const restrictedEnv = {
    ...env,
    DEVICE_SESSIONS: JSON.stringify({ secret: restrictedSession })
  };
  const fetchImpl = async (input) => {
    const url = String(input);
    if (url.startsWith("https://castalia.example")) {
      return response({
        aphorism: { title: "Private", summary: "Not granted" },
        astrology: { title: "Private", summary: "Not granted" },
        synastryWeather: { title: "Private", summary: "Not granted" },
        family: [{ name: "Private", status: "Not granted" }],
        fortune: { title: "Private", summary: "Not granted" },
        card: { title: "Private", summary: "Not granted" }
      });
    }
    if (url.includes("/api/calendars/")) return response([]);
    if (url.endsWith("media_player.kitchen")) return response({ state: "idle", attributes: {} });
    if (url.endsWith("weather.home")) return response({ state: "sunny", attributes: { temperature: 70 } });
    if (url.includes("light.kitchen")) return response({ state: "off", attributes: {} });
    throw new Error(`unexpected ${url}`);
  };
  const gateway = createGateway({ fetchImpl });
  const result = await gateway.fetch(request("/api/v1/device/home?room=Kitchen"), restrictedEnv);
  const body = await result.json();

  assert.equal(body.day.aphorism, null);
  assert.equal(body.selfWeather, null);
  assert.equal(body.synastryWeather, null);
  assert.deepEqual(body.family, []);
  assert.equal(body.fortune, null);
  assert.equal(body.card, null);
});

test("light action calls the Home Assistant light service for the learned room", async () => {
  let serviceCall;
  const gateway = createGateway({
    fetchImpl: async (input, options) => {
      serviceCall = { url: String(input), options };
      return response([]);
    }
  });
  const result = await gateway.fetch(
    request("/api/v1/device/actions", {
      method: "POST",
      body: JSON.stringify({ action: "lights.toggle", room: "Kitchen" })
    }),
    env
  );

  assert.equal(result.status, 200);
  assert.equal(serviceCall.url, "https://ha.example/api/services/light/toggle");
  assert.deepEqual(JSON.parse(serviceCall.options.body), { entity_id: ["light.kitchen_1", "light.kitchen_2"] });
});

test("context actions control Spotify tracks and room light brightness", async () => {
  const serviceCalls = [];
  const gateway = createGateway({
    fetchImpl: async (input, options) => {
      serviceCalls.push({ url: String(input), body: JSON.parse(options.body) });
      return response([]);
    }
  });

  for (const action of ["spotify.previous", "spotify.next", "lights.dimmer", "lights.brighter"]) {
    const result = await gateway.fetch(
      request("/api/v1/device/actions", {
        method: "POST",
        body: JSON.stringify({ action, room: "Kitchen" })
      }),
      env
    );
    assert.equal(result.status, 200);
  }

  assert.equal(serviceCalls[0].url, "https://ha.example/api/services/media_player/media_previous_track");
  assert.equal(serviceCalls[1].url, "https://ha.example/api/services/media_player/media_next_track");
  assert.deepEqual(serviceCalls[2].body, {
    entity_id: ["light.kitchen_1", "light.kitchen_2"],
    brightness_step_pct: -10
  });
  assert.deepEqual(serviceCalls[3].body, {
    entity_id: ["light.kitchen_1", "light.kitchen_2"],
    brightness_step_pct: 10
  });
});

test("eInq select responses ask Al for text without requesting audio", async () => {
  let call;
  const gateway = createGateway({
    fetchImpl: async (input, options) => {
      call = { url: String(input), options };
      return response({ transcript: "Tell me more.", reply: "The current is shifting by the reeds." });
    }
  });
  const result = await gateway.fetch(
    request("/api/v1/device/actions", {
      method: "POST",
      body: JSON.stringify({ action: "al.respond", message: "Tell me more.", room: "Kitchen" })
    }),
    env
  );

  assert.equal(result.status, 200);
  assert.deepEqual(await result.json(), {
    ok: true,
    transcript: "Tell me more.",
    reply: "The current is shifting by the reeds."
  });
  assert.equal(call.url, "https://supabase.example/functions/v1/voice-pipeline");
  assert.deepEqual(JSON.parse(call.options.body), {
    message: "Tell me more.",
    face: "alpheus",
    textOnly: true
  });
  assert.equal(call.options.headers["x-alpheus-source"], "einq");
});

test("Codex actions select a task before forwarding its context action", async () => {
  const calls = [];
  const gateway = createGateway({
    fetchImpl: async (input, options) => {
      calls.push({ url: String(input), options });
      return response({ ok: true });
    }
  });
  const result = await gateway.fetch(
    request("/api/v1/device/actions", {
      method: "POST",
      body: JSON.stringify({ action: "codex.approve", taskId: "thread-1" })
    }),
    { ...env, CODEX_SERVICE_URL: "http://astrolabe.local" }
  );

  assert.equal(result.status, 200);
  assert.equal(calls.length, 2);
  assert.deepEqual(JSON.parse(calls[0].options.body), { action: "select", task_id: "thread-1" });
  assert.deepEqual(JSON.parse(calls[1].options.body), { action: "approve" });
  assert.equal(calls[0].options.headers["X-Astrolabe-Codex"], "sync-v1");
});

test("Codex continue resumes the selected thread through the companion open action", async () => {
  const calls = [];
  const gateway = createGateway({
    fetchImpl: async (input, options) => {
      calls.push({ url: String(input), options });
      return response({ ok: true });
    }
  });
  const result = await gateway.fetch(
    request("/api/v1/device/actions", {
      method: "POST",
      body: JSON.stringify({ action: "codex.continue", taskId: "thread-2" })
    }),
    { ...env, CODEX_SERVICE_URL: "http://astrolabe.local" }
  );

  assert.equal(result.status, 200);
  assert.deepEqual(JSON.parse(calls[0].options.body), { action: "select", task_id: "thread-2" });
  assert.deepEqual(JSON.parse(calls[1].options.body), { action: "open" });
});

test("missing or ungranted device actions are rejected", async () => {
  const restricted = {
    ...env,
    DEVICE_SESSIONS: JSON.stringify({
      secret: { ...session, permissions: { ...session.permissions, lightControl: false } }
    })
  };
  const gateway = createGateway({ fetchImpl: async () => assert.fail("HA must not be called") });
  const result = await gateway.fetch(
    request("/api/v1/device/actions", {
      method: "POST",
      body: JSON.stringify({ action: "lights.toggle", room: "Kitchen" })
    }),
    restricted
  );
  assert.equal(result.status, 403);
});

test("an unknown room cannot control the household default room", async () => {
  const gateway = createGateway({ fetchImpl: async () => assert.fail("HA must not be called") });
  const result = await gateway.fetch(
    request("/api/v1/device/actions", {
      method: "POST",
      body: JSON.stringify({ action: "lights.toggle", room: "Garage" })
    }),
    env
  );
  assert.equal(result.status, 403);
});

test("unknown device tokens are rejected", async () => {
  const gateway = createGateway({ fetchImpl: async () => response({ error: "bad jwt" }, 401) });
  const result = await gateway.fetch(
    new Request("https://mynah.example/api/v1/device/home", {
      headers: { authorization: "Bearer wrong" }
    }),
    env
  );
  assert.equal(result.status, 401);
});

test("Supabase sessions map an authenticated household user to device policy", async () => {
  const mapped = {
    ...env,
    DEVICE_SESSIONS: "{}",
    DEVICE_USERS: JSON.stringify({ "user-1": { ...session, profile: "parent" } })
  };
  const fetchImpl = async (input) => {
    const url = String(input);
    if (url.endsWith("/auth/v1/user")) return response({ id: "user-1" });
    if (url.startsWith("https://castalia.example")) return response({});
    if (url.includes("/api/calendars/")) return response([]);
    if (url.endsWith("media_player.kitchen")) return response({ state: "idle", attributes: {} });
    if (url.endsWith("weather.home")) return response({ state: "sunny", attributes: { temperature: 70 } });
    if (url.includes("light.kitchen")) return response({ state: "off", attributes: {} });
    throw new Error(`unexpected ${url}`);
  };
  const gateway = createGateway({ fetchImpl });
  const result = await gateway.fetch(request("/api/v1/device/home?room=Kitchen"), mapped);
  assert.equal(result.status, 200);
  assert.equal((await result.json()).profile, "parent");
});

test("authenticated users can use a least-privilege default device policy", async () => {
  const mapped = {
    ...env,
    DEVICE_SESSIONS: "{}",
    DEVICE_USERS: "{}",
    DEVICE_DEFAULT_POLICY: JSON.stringify({
      profile: "parent",
      individual: "dcmcshan",
      permissions: { calendar: false, lightControl: false, spotifyControl: false }
    }),
    CASTALIA_CONTENT_API_KEY: "daily-key"
  };
  let contentRequest;
  const fetchImpl = async (input, options = {}) => {
    const url = String(input);
    if (url.endsWith("/auth/v1/user")) return response({ id: "user-without-explicit-policy" });
    if (url.startsWith("https://castalia.example")) {
      contentRequest = options;
      return response({});
    }
    throw new Error(`unexpected ${url}`);
  };
  const gateway = createGateway({ fetchImpl });
  const result = await gateway.fetch(request("/api/v1/device/home"), mapped);
  assert.equal(result.status, 200);
  assert.equal((await result.json()).profile, "parent");
  assert.equal(contentRequest.method, "POST");
  assert.equal(contentRequest.headers["x-api-key"], "daily-key");
  assert.equal(contentRequest.headers["x-castalia-username"], "dcmcshan");
  assert.deepEqual(JSON.parse(contentRequest.body), {
    username: "dcmcshan",
    editorial: true,
    profile: "parent",
    ageBand: "",
    room: ""
  });
});

test("session refresh exchanges a refresh token without exposing the Supabase key", async () => {
  let upstream;
  const gateway = createGateway({
    fetchImpl: async (input, options) => {
      upstream = { url: String(input), options };
      return response({ access_token: "new-access", refresh_token: "new-refresh", expires_in: 7200 });
    }
  });
  const result = await gateway.fetch(
    new Request("https://mynah.example/api/v1/device/session/refresh", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ refresh_token: "old-refresh" })
    }),
    env
  );
  assert.equal(result.status, 200);
  assert.deepEqual(await result.json(), {
    access_token: "new-access",
    refresh_token: "new-refresh",
    expires_in: 7200
  });
  assert.equal(upstream.url, "https://supabase.example/auth/v1/token?grant_type=refresh_token");
  assert.equal(upstream.options.headers.apikey, "anon-key");
});
