export enum OptionKindClass {
  GroupClass = 0,
  InputClass = 1,
  UnknownClass = 2,
  FlagClass = 3,
  JoinedClass = 4,
  ValuesClass = 5,
  SeparateClass = 6,
  CommaJoinedClass = 7,
  MultiArgClass = 8,
  JoinedOrSeparateClass = 9,
  JoinedAndSeparateClass = 10,
  RemainingArgsClass = 11,
  RemainingArgsJoinedClass = 12,
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
