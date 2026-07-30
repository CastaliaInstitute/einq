const jsonHeaders = {
  "content-type": "application/json; charset=utf-8",
  "cache-control": "no-store"
};

function json(value, status = 200) {
  return new Response(JSON.stringify(value), { status, headers: jsonHeaders });
}

function parseSessions(env) {
  try {
    const sessions = JSON.parse(env.DEVICE_SESSIONS || "{}");
    return sessions && typeof sessions === "object" ? sessions : {};
  } catch {
    return {};
  }
}

async function authenticate(request, env, fetchImpl) {
  const header = request.headers.get("authorization") || "";
  const token = header.startsWith("Bearer ") ? header.slice(7).trim() : "";
  if (!token) return null;

  const deviceSession = parseSessions(env)[token];
  if (deviceSession) return deviceSession;
  if (!env.SUPABASE_URL || !env.SUPABASE_ANON_KEY) return null;

  try {
    const user = await fetchJson(fetchImpl, `${env.SUPABASE_URL.replace(/\/+$/, "")}/auth/v1/user`, {
      headers: {
        authorization: `Bearer ${token}`,
        apikey: env.SUPABASE_ANON_KEY
      }
    });
    const users = JSON.parse(env.DEVICE_USERS || "{}");
    return users[user.id] || null;
  } catch {
    return null;
  }
}

function roomConfig(session, requestedRoom) {
  const rooms = session.rooms || {};
  if (requestedRoom) {
    return rooms[requestedRoom] ? { name: requestedRoom, ...rooms[requestedRoom] } : { name: requestedRoom };
  }
  const first = Object.keys(rooms)[0];
  return first ? { name: first, ...rooms[first] } : { name: "" };
}

async function fetchJson(fetchImpl, url, options = {}) {
  const response = await fetchImpl(url, options);
  if (!response.ok) throw new Error(`upstream ${response.status}`);
  return response.json();
}

function haHeaders(env) {
  return {
    authorization: `Bearer ${env.HOME_ASSISTANT_TOKEN}`,
    "content-type": "application/json"
  };
}

async function haGet(fetchImpl, env, path) {
  if (!env.HOME_ASSISTANT_URL || !env.HOME_ASSISTANT_TOKEN) return null;
  const base = env.HOME_ASSISTANT_URL.replace(/\/+$/, "");
  try {
    return await fetchJson(fetchImpl, `${base}${path}`, { headers: haHeaders(env) });
  } catch {
    return null;
  }
}

async function haService(fetchImpl, env, domain, service, entityIds, data = {}) {
  if (!env.HOME_ASSISTANT_URL || !env.HOME_ASSISTANT_TOKEN || !entityIds?.length) return false;
  const base = env.HOME_ASSISTANT_URL.replace(/\/+$/, "");
  const response = await fetchImpl(`${base}/api/services/${domain}/${service}`, {
    method: "POST",
    headers: haHeaders(env),
    body: JSON.stringify({ entity_id: entityIds, ...data })
  });
  return response.ok;
}

async function contentFor(fetchImpl, env, session, room) {
  if (!env.CASTALIA_CONTENT_URL) return {};
  const url = new URL(env.CASTALIA_CONTENT_URL);
  url.searchParams.set("profile", session.profile || "parent");
  if (session.ageBand) url.searchParams.set("ageBand", session.ageBand);
  if (room.name) url.searchParams.set("room", room.name);
  const headers = { accept: "application/json" };
  if (env.CASTALIA_SERVICE_TOKEN) headers.authorization = `Bearer ${env.CASTALIA_SERVICE_TOKEN}`;
  try {
    return await fetchJson(fetchImpl, url, { headers });
  } catch {
    return {};
  }
}

async function newsFor(fetchImpl, env, session) {
  if (!env.CASTALIA_NEWS_URL) return null;
  const url = new URL(env.CASTALIA_NEWS_URL);
  url.searchParams.set("limit", "1");
  if (session.ageBand) url.searchParams.set("ageBand", session.ageBand);
  const headers = { accept: "application/json" };
  if (env.CASTALIA_SERVICE_TOKEN) headers.authorization = `Bearer ${env.CASTALIA_SERVICE_TOKEN}`;
  try {
    const result = await fetchJson(fetchImpl, url, { headers });
    const item = Array.isArray(result) ? result[0] : result.items?.[0] || result.article || result;
    if (!item || typeof item !== "object") return null;
    return {
      title: item.title || "",
      summary: item.summary || item.description || "",
      byline: item.byline || item.author || "",
      source: item.source || "news.castalia.institute"
    };
  } catch {
    return null;
  }
}

function calendarEntity(session, requestedCalendar) {
  const calendars = session.calendars || {};
  if (requestedCalendar) return calendars[requestedCalendar] || null;
  if (session.defaultCalendarId && calendars[session.defaultCalendarId]) {
    return calendars[session.defaultCalendarId];
  }
  return session.calendarEntity || null;
}

async function nextEvent(fetchImpl, env, session, requestedCalendar, now) {
  const entity = calendarEntity(session, requestedCalendar);
  if (!entity || session.permissions?.calendar === false) return null;
  const end = new Date(now.getTime() + 24 * 60 * 60 * 1000);
  const path = `/api/calendars/${encodeURIComponent(entity)}?start=${encodeURIComponent(
    now.toISOString()
  )}&end=${encodeURIComponent(end.toISOString())}`;
  const events = await haGet(fetchImpl, env, path);
  if (!Array.isArray(events) || !events.length) return null;
  const event = events[0];
  const privateTitle = session.profile === "kid" && session.calendarTitleMode !== "full";
  return {
    title: privateTitle ? "Busy" : event.summary || event.title || "Calendar event",
    start: event.start?.dateTime || event.start?.date || event.start || "",
    end: event.end?.dateTime || event.end?.date || event.end || "",
    allDay: Boolean(event.start?.date || /^\d{4}-\d{2}-\d{2}$/.test(event.start || ""))
  };
}

async function spotifyState(fetchImpl, env, room) {
  if (!room.spotifyEntity) return { connected: false };
  const state = await haGet(fetchImpl, env, `/api/states/${encodeURIComponent(room.spotifyEntity)}`);
  if (!state || state.state === "unavailable") return { connected: false };
  const attributes = state.attributes || {};
  return {
    connected: true,
    playing: state.state === "playing",
    track: attributes.media_title || "",
    artist: attributes.media_artist || "",
    device: attributes.friendly_name || room.name || "",
    volume: Math.round((attributes.volume_level || 0) * 100)
  };
}

async function weatherState(fetchImpl, env, session) {
  if (!session.weatherEntity) return { condition: "", temperature: "", summary: "" };
  const state = await haGet(fetchImpl, env, `/api/states/${encodeURIComponent(session.weatherEntity)}`);
  if (!state || state.state === "unavailable") return { condition: "", temperature: "", summary: "" };
  const attributes = state.attributes || {};
  const value = attributes.temperature;
  const unit = attributes.temperature_unit || "";
  return {
    condition: state.state || "",
    temperature: Number.isFinite(value) ? `${Math.round(value)}${unit}` : "",
    summary: attributes.friendly_name || ""
  };
}

async function lightState(fetchImpl, env, room) {
  const entityIds = room.lightEntities || [];
  if (!entityIds.length) return { available: false, room: room.name || "" };
  const states = await Promise.all(
    entityIds.map((entityId) => haGet(fetchImpl, env, `/api/states/${encodeURIComponent(entityId)}`))
  );
  const available = states.filter((state) => state && state.state !== "unavailable");
  if (!available.length) return { available: false, room: room.name || "" };
  const on = available.filter((state) => state.state === "on");
  const brightnessValues = on
    .map((state) => state.attributes?.brightness)
    .filter((value) => Number.isFinite(value));
  return {
    available: true,
    room: room.name || "",
    on: on.length > 0,
    brightness: brightnessValues.length
      ? Math.round((brightnessValues.reduce((sum, value) => sum + value, 0) / brightnessValues.length / 255) * 100)
      : on.length
        ? 100
        : 0,
    scene: room.scene || ""
  };
}

function permissionsFor(session) {
  const granted = session.permissions || {};
  return {
    calendar: granted.calendar !== false,
    astrology: granted.astrology !== false,
    fortune: granted.fortune !== false,
    cards: granted.cards !== false,
    spotifyControl: granted.spotifyControl === true,
    lightControl: granted.lightControl === true,
    administration: session.profile === "parent" && granted.administration === true
  };
}

async function homePayload(fetchImpl, env, session, roomName, requestedCalendar, now) {
  const room = roomConfig(session, roomName);
  const permissions = permissionsFor(session);
  const [content, news, event, weather, spotify, lights] = await Promise.all([
    contentFor(fetchImpl, env, session, room),
    newsFor(fetchImpl, env, session),
    nextEvent(fetchImpl, env, session, requestedCalendar, now),
    weatherState(fetchImpl, env, session),
    spotifyState(fetchImpl, env, room),
    lightState(fetchImpl, env, room)
  ]);
  const castaliaEvent = content.calendar?.events?.[0] || content.today?.nextEvent || null;
  const tasks = Array.isArray(content.tasks)
    ? content.tasks
    : Array.isArray(content.calendar?.tasks)
      ? content.calendar.tasks
      : [];
  return {
    schema: "castalia.device.daily.v1",
    date: now.toISOString().slice(0, 10),
    generatedAt: now.toISOString(),
    profile: session.profile === "kid" ? "kid" : "parent",
    today: {
      nextEvent: permissions.calendar ? castaliaEvent || event : null,
      tasks: permissions.calendar ? tasks.slice(0, 6) : []
    },
    weather,
    day: { aphorism: permissions.astrology ? content.day?.aphorism || content.aphorism || null : null },
    selfWeather: permissions.astrology ? content.selfWeather || content.astrology || null : null,
    synastryWeather: permissions.astrology ? content.synastryWeather || null : null,
    family: permissions.astrology && Array.isArray(content.family) ? content.family.slice(0, 6) : [],
    fortune: permissions.fortune ? content.fortune || null : null,
    card: permissions.cards ? content.card || null : null,
    news: content.news || news,
    art: content.art || content.artOfTheDay || null,
    quote: content.quote || content.quoteOfTheDay || null,
    mindfulness: content.mindfulness || null,
    library: content.library || null,
    settings: content.settings || null,
    ota: content.ota || null,
    spotify,
    lights,
    permissions
  };
}

async function performAction(fetchImpl, env, session, action, roomName) {
  const room = roomConfig(session, roomName);
  const permissions = permissionsFor(session);
  if (action === "spotify.toggle" && permissions.spotifyControl) {
    return haService(fetchImpl, env, "media_player", "media_play_pause", [room.spotifyEntity].filter(Boolean));
  }
  if (action === "spotify.previous" && permissions.spotifyControl) {
    return haService(fetchImpl, env, "media_player", "media_previous_track", [room.spotifyEntity].filter(Boolean));
  }
  if (action === "spotify.next" && permissions.spotifyControl) {
    return haService(fetchImpl, env, "media_player", "media_next_track", [room.spotifyEntity].filter(Boolean));
  }
  if (action === "lights.toggle" && permissions.lightControl) {
    return haService(fetchImpl, env, "light", "toggle", room.lightEntities || []);
  }
  if (action === "lights.dimmer" && permissions.lightControl) {
    return haService(fetchImpl, env, "light", "turn_on", room.lightEntities || [], { brightness_step_pct: -10 });
  }
  if (action === "lights.brighter" && permissions.lightControl) {
    return haService(fetchImpl, env, "light", "turn_on", room.lightEntities || [], { brightness_step_pct: 10 });
  }
  return false;
}

async function refreshSession(fetchImpl, env, request) {
  if (!env.SUPABASE_URL || !env.SUPABASE_ANON_KEY) {
    return json({ error: "refresh_unavailable" }, 503);
  }
  let body;
  try {
    body = await request.json();
  } catch {
    return json({ error: "invalid_json" }, 400);
  }
  if (typeof body.refresh_token !== "string" || !body.refresh_token) {
    return json({ error: "refresh_token_required" }, 400);
  }

  try {
    const tokens = await fetchJson(
      fetchImpl,
      `${env.SUPABASE_URL.replace(/\/+$/, "")}/auth/v1/token?grant_type=refresh_token`,
      {
        method: "POST",
        headers: {
          authorization: `Bearer ${env.SUPABASE_ANON_KEY}`,
          apikey: env.SUPABASE_ANON_KEY,
          "content-type": "application/json"
        },
        body: JSON.stringify({ refresh_token: body.refresh_token })
      }
    );
    return json({
      access_token: tokens.access_token,
      refresh_token: tokens.refresh_token,
      expires_in: tokens.expires_in || 3600
    });
  } catch {
    return json({ error: "refresh_failed" }, 401);
  }
}

export function createGateway({ fetchImpl = globalThis.fetch, now = () => new Date() } = {}) {
  return {
    async fetch(request, env) {
      const url = new URL(request.url);
      const apiOffset = url.pathname.indexOf("/api/v1/");
      const path = apiOffset >= 0 ? url.pathname.slice(apiOffset) : url.pathname;
      if (request.method === "GET" && path === "/api/v1/health") {
        return json({ ok: true, service: "mynah-gateway", apiVersion: 1 });
      }
      if (request.method === "POST" && path === "/api/v1/device/session/refresh") {
        return refreshSession(fetchImpl, env, request);
      }

      const session = await authenticate(request, env, fetchImpl);
      if (!session) return json({ error: "unauthorized" }, 401);

      if (
        request.method === "GET" &&
        (path === "/api/v1/device/home" || path === "/api/v1/device/daily")
      ) {
        return json(
          await homePayload(
            fetchImpl,
            env,
            session,
            url.searchParams.get("room"),
            url.searchParams.get("calendar"),
            now()
          )
        );
      }

      if (request.method === "POST" && path === "/api/v1/device/actions") {
        let body;
        try {
          body = await request.json();
        } catch {
          return json({ error: "invalid_json" }, 400);
        }
        const accepted = await performAction(fetchImpl, env, session, body.action, body.room);
        return accepted ? json({ ok: true }) : json({ error: "forbidden_or_unavailable" }, 403);
      }

      return json({ error: "not_found" }, 404);
    }
  };
}

export default createGateway();
