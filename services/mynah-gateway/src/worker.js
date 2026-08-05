const jsonHeaders = {
  "content-type": "application/json; charset=utf-8",
  "cache-control": "no-store"
};

function json(value, status = 200) {
  return new Response(JSON.stringify(value), { status, headers: jsonHeaders });
}

function parseObject(value) {
  try {
    const parsed = JSON.parse(value || "{}");
    return parsed && typeof parsed === "object" && !Array.isArray(parsed) ? parsed : {};
  } catch {
    return {};
  }
}

function parseSessions(env) {
  return parseObject(env.DEVICE_SESSIONS);
}

function timeZoneFor(env, session) {
  const username = String(session.individual || session.username || "");
  const configured = parseObject(env.DEVICE_TIME_ZONES_JSON)[username];
  const candidate = String(session.timezone || configured || env.DEFAULT_TIME_ZONE || "UTC");
  try {
    new Intl.DateTimeFormat("en", { timeZone: candidate }).format(0);
    return candidate;
  } catch {
    return "UTC";
  }
}

function issueDateFor(now, timeZone) {
  const parts = new Intl.DateTimeFormat("en-US", {
    timeZone,
    year: "numeric",
    month: "2-digit",
    day: "2-digit"
  }).formatToParts(now);
  const values = Object.fromEntries(parts.map(({ type, value }) => [type, value]));
  return `${values.year}-${values.month}-${values.day}`;
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
    const users = parseObject(env.DEVICE_USERS);
    const policy = users[user.id] || parseObject(env.DEVICE_DEFAULT_POLICY);
    return Object.keys(policy).length ? { ...policy, userId: user.id } : null;
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
  const username = session.individual || session.username || "";
  const headers = { accept: "application/json", "content-type": "application/json" };
  if (env.CASTALIA_CONTENT_API_KEY) headers["x-api-key"] = env.CASTALIA_CONTENT_API_KEY;
  else if (env.CASTALIA_SERVICE_TOKEN) headers.authorization = `Bearer ${env.CASTALIA_SERVICE_TOKEN}`;
  if (username) headers["x-castalia-username"] = username;
  try {
    return await fetchJson(fetchImpl, url, {
      method: "POST",
      headers,
      body: JSON.stringify({
        username,
        editorial: true,
        profile: session.profile || "parent",
        ageBand: session.ageBand || "",
        room: room.name || ""
      })
    });
  } catch {
    return {};
  }
}

function familyRepositoryFor(env, session) {
  const username = String(session.individual || session.username || "");
  const mapping = parseObject(env.FAMILY_RHYTHM_REPO_MAP_JSON);
  const mapped = mapping[username];
  const repository = String(
    session.familyRepository ||
      (typeof mapped === "string" ? mapped : mapped?.repository || mapped?.repo || "")
  );
  return /^CastaliaInstitute\/castalia-family-[A-Za-z0-9._-]+$/.test(repository) ? repository : "";
}

async function familyGazetteerFor(fetchImpl, env, session, now) {
  const username = String(session.individual || session.username || "");
  const date = issueDateFor(now, timeZoneFor(env, session));
  if (username && env.SUPABASE_URL && env.SUPABASE_SERVICE_ROLE_KEY) {
    const base = env.SUPABASE_URL.replace(/\/+$/, "");
    const query = new URL(`${base}/rest/v1/castalia_device_daily_editions`);
    query.searchParams.set("username", `eq.${username}`);
    query.searchParams.set("issue_date", `eq.${date}`);
    query.searchParams.set("select", "payload");
    query.searchParams.set("limit", "1");
    try {
      const rows = await fetchJson(fetchImpl, query, {
        headers: {
          apikey: env.SUPABASE_SERVICE_ROLE_KEY,
          authorization: `Bearer ${env.SUPABASE_SERVICE_ROLE_KEY}`
        }
      });
      const payload = Array.isArray(rows) ? rows[0]?.payload : null;
      if (payload && typeof payload === "object" && !Array.isArray(payload)) return payload;
    } catch {
      // A private GitHub family archive remains a migration fallback.
    }
  }

  const repository = familyRepositoryFor(env, session);
  const token = env.FAMILY_RHYTHM_GITHUB_TOKEN || env.GITHUB_LIBRARY_TOKEN;
  if (!repository || !token) return null;
  const path = `outputs/${date}/daily-content.json`;
  const encodedPath = path.split("/").map(encodeURIComponent).join("/");
  const reference = encodeURIComponent(env.FAMILY_RHYTHM_GITHUB_REF || "main");
  try {
    const response = await fetchImpl(
      `https://api.github.com/repos/${repository}/contents/${encodedPath}?ref=${reference}`,
      {
        headers: {
          accept: "application/vnd.github.raw+json",
          authorization: `Bearer ${token}`,
          "user-agent": "Castalia-Mynah-Gazetteer/1"
        }
      }
    );
    if (!response.ok) return null;
    const contentLength = Number(response.headers.get("content-length") || 0);
    if (contentLength > 128 * 1024) return null;
    const payload = await response.json();
    return payload && typeof payload === "object" && !Array.isArray(payload) ? payload : null;
  } catch {
    return null;
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

function normalizedScriptorium(catalog, repository, revision = "") {
  if (!catalog || catalog.schema !== "castalia.individual.library.v1" || !Array.isArray(catalog.books)) {
    return null;
  }
  const books = catalog.books
    .filter((book) => {
      const path = typeof book?.path === "string" ? book.path : "";
      return book && typeof book === "object" && path.startsWith("library/books/") && path.endsWith(".epub");
    })
    .slice(0, 8)
    .map((book) => ({
      id: String(book.id || "").slice(0, 48),
      title: String(book.title || "Untitled").slice(0, 72),
      authors: (Array.isArray(book.authors) ? book.authors : [])
        .map((author) => String(author))
        .join(", ")
        .slice(0, 80),
      path: String(book.path).slice(0, 128)
    }));
  return {
    schema: catalog.schema,
    repository,
    revision: String(revision || catalog.revision || "").replace(/^W\//, "").replaceAll('"', "").slice(0, 48),
    format: "epub",
    bookCount: catalog.books.length,
    changedCount: 0,
    books
  };
}

async function scriptoriumFor(fetchImpl, env, session) {
  const individual = String(session.individual || session.username || "");
  const repository = String(
    session.libraryRepository || (individual ? `CastaliaInstitute/castalia-${individual}` : "")
  );
  // Repository selection is policy-derived, never supplied by the device.
  if (!/^CastaliaInstitute\/castalia-[A-Za-z0-9._-]+$/.test(repository)) return null;
  if (!env.GITHUB_LIBRARY_TOKEN) return null;
  try {
    const response = await fetchImpl(
      `https://api.github.com/repos/${repository}/contents/library/catalog.json`,
      {
        headers: {
          accept: "application/vnd.github.raw+json",
          authorization: `Bearer ${env.GITHUB_LIBRARY_TOKEN}`,
          "user-agent": "Castalia-Mynah-Scriptorium/1"
        }
      }
    );
    if (!response.ok) return null;
    return normalizedScriptorium(await response.json(), repository, response.headers.get("etag") || "");
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
    codexControl: granted.codexControl === true,
    administration: session.profile === "parent" && granted.administration === true
  };
}

function gazetteerItem(title = "", summary = "", byline = "", detail = "") {
  return { title: title || "", summary: summary || "", byline: byline || "", detail: detail || "" };
}

function gazetteerFor(content) {
  const edition = content?.content && typeof content.content === "object" ? content : null;
  const daily = edition?.content || {};
  if (!edition) return null;
  return {
    season: edition.season || "",
    theme: edition.theme?.message || "",
    book: gazetteerItem(daily.book?.title, daily.book?.why, daily.book?.creator),
    quote: gazetteerItem("Quote of the Day", daily.quote?.text, daily.quote?.attribution),
    poem: gazetteerItem(daily.poem?.title, daily.poem?.excerpt, daily.poem?.creator, daily.poem?.note),
    faculty: gazetteerItem(daily.faculty?.name, daily.faculty?.practice, "Faculty of the Day", daily.faculty?.astrology_note),
    history: gazetteerItem(daily.history?.title, daily.history?.note, daily.history?.date),
    country: gazetteerItem(daily.country?.name, daily.country?.reflection || daily.country?.thread, daily.country?.capital),
    art: gazetteerItem(daily.art?.title, daily.art?.looking_prompt || daily.art?.thread, daily.art?.artist, daily.art?.medium),
    bible: gazetteerItem(daily.bible?.reference, daily.bible?.text, "Bible of the Day", daily.bible?.reflection)
  };
}

async function homePayload(fetchImpl, env, session, roomName, requestedCalendar, now) {
  const room = roomConfig(session, roomName);
  const permissions = permissionsFor(session);
  const [upstreamContent, familyGazetteer, news, event, weather, spotify, lights, scriptorium] = await Promise.all([
    contentFor(fetchImpl, env, session, room),
    familyGazetteerFor(fetchImpl, env, session, now),
    newsFor(fetchImpl, env, session),
    nextEvent(fetchImpl, env, session, requestedCalendar, now),
    weatherState(fetchImpl, env, session),
    spotifyState(fetchImpl, env, room),
    lightState(fetchImpl, env, room),
    scriptoriumFor(fetchImpl, env, session)
  ]);
  const content = familyGazetteer ? { ...upstreamContent, ...familyGazetteer } : upstreamContent;
  const castaliaEvent = content.calendar?.events?.[0] || content.today?.nextEvent || null;
  const tasks = Array.isArray(content.tasks)
    ? content.tasks
    : Array.isArray(content.calendar?.tasks)
      ? content.calendar.tasks
      : [];
  const gazetteer = gazetteerFor(content);
  return {
    schema: "castalia.device.daily.v1",
    date: issueDateFor(now, timeZoneFor(env, session)),
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
    astrologyMeta: permissions.astrology ? content.astrologyMeta || null : null,
    family: permissions.astrology && Array.isArray(content.family) ? content.family.slice(0, 6) : [],
    fortune: permissions.fortune ? content.fortune || null : null,
    card: permissions.cards ? content.card || null : null,
    news: content.news || news,
    art: gazetteer?.art || content.art || content.artOfTheDay || null,
    quote: gazetteer?.quote || content.quote || content.quoteOfTheDay || null,
    gazetteer: gazetteer
      ? {
          season: gazetteer.season,
          theme: gazetteer.theme,
          book: gazetteer.book,
          poem: gazetteer.poem,
          faculty: gazetteer.faculty,
          history: gazetteer.history,
          country: gazetteer.country,
          bible: gazetteer.bible
        }
      : null,
    mindfulness: content.mindfulness || null,
    library: scriptorium || content.library || null,
    codex: content.codex || null,
    settings: content.settings || null,
    ota: content.ota || null,
    spotify,
    lights,
    permissions
  };
}

function payloadSegment(payload, segment) {
  if (!segment) return payload;
  const metadata = {
    schema: payload.schema,
    date: payload.date,
    generatedAt: payload.generatedAt,
    profile: payload.profile
  };
  const gazetteer = payload.gazetteer || {};
  if (segment === "core") {
    const core = { ...metadata, permissions: payload.permissions };
    if (payload.today?.nextEvent || payload.today?.tasks?.length) core.today = payload.today;
    if (payload.weather?.condition || payload.weather?.temperature) core.weather = payload.weather;
    if (payload.day?.aphorism) core.day = payload.day;
    for (const key of ["fortune", "card", "news", "mindfulness", "codex", "settings", "ota"]) {
      if (payload[key] && (typeof payload[key] !== "object" || Object.keys(payload[key]).length)) {
        core[key] = payload[key];
      }
    }
    if (payload.spotify?.connected) core.spotify = payload.spotify;
    if (payload.lights?.available) core.lights = payload.lights;
    return core;
  }
  if (segment === "astrology-self") {
    return {
      ...metadata,
      astrologyMeta: payload.astrologyMeta,
      selfWeather: payload.selfWeather
    };
  }
  if (segment === "astrology-synastry") {
    return {
      ...metadata,
      astrologyMeta: payload.astrologyMeta,
      synastryWeather: payload.synastryWeather,
      family: payload.family
    };
  }
  if (segment === "scriptorium") {
    return {
      ...metadata,
      library: payload.library
    };
  }
  if (segment === "daily-1") {
    return {
      ...metadata,
      art: payload.art,
      gazetteer: {
        season: gazetteer.season || "",
        theme: gazetteer.theme || "",
        book: gazetteer.book || null
      }
    };
  }
  if (segment === "daily-2") {
    return {
      ...metadata,
      quote: payload.quote,
      gazetteer: {
        poem: gazetteer.poem || null
      }
    };
  }
  if (segment === "daily-3") {
    return {
      ...metadata,
      gazetteer: {
        faculty: gazetteer.faculty || null,
        history: gazetteer.history || null
      }
    };
  }
  if (segment === "daily-4") {
    return {
      ...metadata,
      gazetteer: {
        country: gazetteer.country || null,
        bible: gazetteer.bible || null
      }
    };
  }
  return null;
}

async function performAction(fetchImpl, env, session, action, roomName, taskId) {
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
  const codexActions = {
    "codex.open": "open",
    "codex.continue": "open",
    "codex.interrupt": "interrupt",
    "codex.approve": "approve",
    "codex.pin-toggle": "pin-toggle"
  };
  if (codexActions[action] && permissions.codexControl && env.CODEX_SERVICE_URL && taskId) {
    const headers = { "content-type": "application/json", "X-Astrolabe-Codex": "sync-v1" };
    if (env.CODEX_SERVICE_TOKEN) headers.authorization = `Bearer ${env.CODEX_SERVICE_TOKEN}`;
    const response = await fetchImpl(
      `${env.CODEX_SERVICE_URL.replace(/\/+$/, "")}/api/codex/action`,
      {
        method: "POST",
        headers,
        body: JSON.stringify({ action: "select", task_id: taskId })
      }
    );
    if (!response.ok) return false;
    const actionResponse = await fetchImpl(
      `${env.CODEX_SERVICE_URL.replace(/\/+$/, "")}/api/codex/action`,
      {
        method: "POST",
        headers,
        body: JSON.stringify({ action: codexActions[action] })
      }
    );
    return actionResponse.ok;
  }
  return false;
}

async function askAlpheus(fetchImpl, env, message) {
  if (!env.SUPABASE_URL || !env.SUPABASE_ANON_KEY) return null;
  const text = typeof message === "string" ? message.trim().slice(0, 240) : "";
  if (!text) return null;
  try {
    return await fetchJson(
      fetchImpl,
      `${env.SUPABASE_URL.replace(/\/+$/, "")}/functions/v1/voice-pipeline`,
      {
        method: "POST",
        headers: {
          authorization: `Bearer ${env.SUPABASE_ANON_KEY}`,
          apikey: env.SUPABASE_ANON_KEY,
          "content-type": "application/json",
          "x-alpheus-source": "einq"
        },
        body: JSON.stringify({ message: text, face: "alpheus", textOnly: true })
      }
    );
  } catch {
    return null;
  }
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
        const payload = await homePayload(
          fetchImpl,
          env,
          session,
          url.searchParams.get("room"),
          url.searchParams.get("calendar"),
          now()
        );
        const segmented = payloadSegment(payload, url.searchParams.get("segment"));
        return segmented ? json(segmented) : json({ error: "invalid_segment" }, 400);
      }

      if (request.method === "POST" && path === "/api/v1/device/actions") {
        let body;
        try {
          body = await request.json();
        } catch {
          return json({ error: "invalid_json" }, 400);
        }
        if (body.action === "al.respond") {
          const answer = await askAlpheus(fetchImpl, env, body.message);
          return answer && typeof answer.reply === "string"
            ? json({ ok: true, transcript: answer.transcript || body.message, reply: answer.reply })
            : json({ error: "al_unavailable" }, 503);
        }
        const accepted = await performAction(fetchImpl, env, session, body.action, body.room, body.taskId);
        return accepted ? json({ ok: true }) : json({ error: "forbidden_or_unavailable" }, 403);
      }

      return json({ error: "not_found" }, 404);
    }
  };
}

export default createGateway();
