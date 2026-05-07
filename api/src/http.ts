import {
  http_client_close,
  http_client_create,
  http_client_request,
} from "catter-c";

export {};

export type HeaderInit = Record<string, string> | Array<[string, string]>;

export type RequestOptions = {
  method?: string;
  headers?: HeaderInit;
  body?: string;
  timeoutMs?: number;
  maxRedirects?: number;
  dangerAcceptInvalidCerts?: boolean;
  dangerAcceptInvalidHostnames?: boolean;
  proxy?: string;
};

export class Response {
  readonly status: number;
  readonly ok: boolean;
  readonly url: string;
  readonly body: string;
  readonly rawHeaders: Array<[string, string]>;
  readonly headers: Record<string, string>;

  constructor(raw: {
    status: number;
    ok: boolean;
    url: string;
    body: string;
    rawHeaders: string[];
  }) {
    this.status = raw.status;
    this.ok = raw.ok;
    this.url = raw.url;
    this.body = raw.body;
    this.rawHeaders = pairs(raw.rawHeaders);
    this.headers = normalizeHeaders(this.rawHeaders);
  }

  text(): string {
    return this.body;
  }

  json<T = unknown>(): T {
    return JSON.parse(this.body) as T;
  }

  header(name: string): string | undefined {
    return this.headers[name.toLowerCase()];
  }
}

export class Client {
  private clientId: number | undefined = http_client_create();

  close(): void {
    if (this.clientId === undefined) {
      return;
    }

    http_client_close(this.clientId);
    this.clientId = undefined;
  }

  async request(url: string, options: RequestOptions = {}): Promise<Response> {
    const clientId = this.requireClient();
    const raw = await http_client_request(
      clientId,
      options.method ?? "GET",
      url,
      flattenHeaders(options.headers),
      options.body ?? "",
      options.timeoutMs ?? -1,
      options.maxRedirects ?? -1,
      options.dangerAcceptInvalidCerts ?? false,
      options.dangerAcceptInvalidHostnames ?? false,
      options.proxy ?? "",
    );
    return new Response(raw);
  }

  get(
    url: string,
    options: Omit<RequestOptions, "method" | "body"> = {},
  ): Promise<Response> {
    return this.request(url, { ...options, method: "GET" });
  }

  head(
    url: string,
    options: Omit<RequestOptions, "method" | "body"> = {},
  ): Promise<Response> {
    return this.request(url, { ...options, method: "HEAD" });
  }

  del(
    url: string,
    options: Omit<RequestOptions, "method"> = {},
  ): Promise<Response> {
    return this.request(url, { ...options, method: "DELETE" });
  }

  post(
    url: string,
    body = "",
    options: Omit<RequestOptions, "method" | "body"> = {},
  ): Promise<Response> {
    return this.request(url, { ...options, method: "POST", body });
  }

  put(
    url: string,
    body = "",
    options: Omit<RequestOptions, "method" | "body"> = {},
  ): Promise<Response> {
    return this.request(url, { ...options, method: "PUT", body });
  }

  patch(
    url: string,
    body = "",
    options: Omit<RequestOptions, "method" | "body"> = {},
  ): Promise<Response> {
    return this.request(url, { ...options, method: "PATCH", body });
  }

  async text(url: string, options: RequestOptions = {}): Promise<string> {
    return (await this.request(url, options)).text();
  }

  async json<T = unknown>(
    url: string,
    options: RequestOptions = {},
  ): Promise<T> {
    return (await this.request(url, options)).json<T>();
  }

  private requireClient(): number {
    if (this.clientId === undefined) {
      throw new Error("HTTP client is closed");
    }
    return this.clientId;
  }
}

let defaultClient: Client | undefined;

function getDefaultClient(): Client {
  defaultClient ??= new Client();
  return defaultClient;
}

export async function request(
  url: string,
  options: RequestOptions = {},
): Promise<Response> {
  return getDefaultClient().request(url, options);
}

export function get(
  url: string,
  options: Omit<RequestOptions, "method" | "body"> = {},
): Promise<Response> {
  return getDefaultClient().get(url, options);
}

export function head(
  url: string,
  options: Omit<RequestOptions, "method" | "body"> = {},
): Promise<Response> {
  return getDefaultClient().head(url, options);
}

export function del(
  url: string,
  options: Omit<RequestOptions, "method"> = {},
): Promise<Response> {
  return getDefaultClient().del(url, options);
}

export function post(
  url: string,
  body = "",
  options: Omit<RequestOptions, "method" | "body"> = {},
): Promise<Response> {
  return getDefaultClient().post(url, body, options);
}

export function put(
  url: string,
  body = "",
  options: Omit<RequestOptions, "method" | "body"> = {},
): Promise<Response> {
  return getDefaultClient().put(url, body, options);
}

export function patch(
  url: string,
  body = "",
  options: Omit<RequestOptions, "method" | "body"> = {},
): Promise<Response> {
  return getDefaultClient().patch(url, body, options);
}

export async function text(
  url: string,
  options: RequestOptions = {},
): Promise<string> {
  return getDefaultClient().text(url, options);
}

export async function json<T = unknown>(
  url: string,
  options: RequestOptions = {},
): Promise<T> {
  return getDefaultClient().json<T>(url, options);
}

function flattenHeaders(headers: HeaderInit | undefined): string[] {
  if (!headers) {
    return [];
  }

  if (Array.isArray(headers)) {
    return headers.flatMap(([name, value]) => [name, value]);
  }

  return Object.entries(headers).flatMap(([name, value]) => [name, value]);
}

function pairs(flatHeaders: string[]): Array<[string, string]> {
  const result: Array<[string, string]> = [];
  for (let i = 0; i + 1 < flatHeaders.length; i += 2) {
    result.push([flatHeaders[i], flatHeaders[i + 1]]);
  }
  return result;
}

function normalizeHeaders(
  headers: Array<[string, string]>,
): Record<string, string> {
  const result: Record<string, string> = {};
  for (const [name, value] of headers) {
    const key = name.toLowerCase();
    result[key] = key in result ? `${result[key]}, ${value}` : value;
  }
  return result;
}
