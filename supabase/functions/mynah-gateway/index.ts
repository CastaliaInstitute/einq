import { createGateway } from "../../../services/mynah-gateway/src/worker.js";

const gateway = createGateway();

Deno.serve((request: Request) => {
  const url = new URL(request.url);
  const apiPath = url.pathname.indexOf("/api/v1/");
  if (apiPath >= 0) url.pathname = url.pathname.slice(apiPath);
  return gateway.fetch(new Request(url, request), Deno.env.toObject());
});
