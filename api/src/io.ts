import * as capi from "catter-c";
/**
 * Provides standard output printing
 * @param content - content to print
 */
export function print(content: string) {
  capi.stdout_print(content);
}
