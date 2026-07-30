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
  assert.equal(body.permissions.administration, false);
  assert.ok(calls.some((call) => call.url.includes("room=Kitchen")));
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
