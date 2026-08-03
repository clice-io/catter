export enum OptionKindClass {
  Group = 0,
  Input = 1,
  Unknown = 2,
  Flag = 3,
  Joined = 4,
  Values = 5,
  Separate = 6,
  CommaJoined = 7,
  MultiArg = 8,
  JoinedOrSeparate = 9,
  JoinedAndSeparate = 10,
  RemainingArgs = 11,
  RemainingArgsJoined = 12,
}

export type OptionInfo = {
  id: number;
  prefixedKey: string;
  kind: OptionKindClass;
  group: number;
  alias: number;
  aliasArgs: string[];
  flags: number;
  visibility: number;
  param: number;
  help: string;
  meta_var: string;
};

export type OptionItem = {
  values: string[];
  key: string;
  id: number;
  index: number;
};

export type OptionTable =
  | "clang"
  | "lld-coff"
  | "lld-elf"
  | "lld-macho"
  | "lld-mingw"
  | "lld-wasm"
  | "nvcc"
  | "llvm-dlltool"
  | "llvm-lib";
