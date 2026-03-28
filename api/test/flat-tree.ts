import { data, debug } from "catter";

function expectEq<T>(actual: T, expected: T, label: string) {
  if (actual !== expected) {
    throw new Error(`${label}: expected ${expected}, got ${actual}`);
  }
}

function expectArrayEq<T>(
  actual: readonly T[],
  expected: readonly T[],
  label: string,
) {
  if (actual.length !== expected.length) {
    throw new Error(
      `${label}: expected [${expected.join(", ")}], got [${actual.join(", ")}]`,
    );
  }

  for (let index = 0; index < actual.length; ++index) {
    if (actual[index] !== expected[index]) {
      throw new Error(
        `${label}: expected [${expected.join(", ")}], got [${actual.join(", ")}]`,
      );
    }
  }
}

function expectThrows(cb: () => void, label: string) {
  let thrown = false;
  try {
    cb();
  } catch {
    thrown = true;
  }
  if (!thrown) {
    throw new Error(`${label}: expected exception`);
  }
}

const basic = new data.FlatTree<number, string>([
  { id: 1, content: "root" },
  { id: 2, parentId: 1, content: "left" },
  { id: 3, parentId: 1, content: "right" },
  { id: 4, parentId: 2, content: "leaf" },
  { id: 5, parentId: 42, content: "orphan" },
]);

expectEq(basic.size, 5, "basic size");
debug.assertThrow(basic.has(1));
debug.assertThrow(!basic.has(99));
expectEq(basic.node(1)?.content, "root", "node lookup");
expectEq(basic.node(99), undefined, "missing node lookup");
expectEq(basic.parent(1), undefined, "root parent");
expectEq(basic.father(4)?.id, 2, "father alias");
expectArrayEq(basic.node(4)?.parentIds ?? [], [2], "single parent ids");
expectArrayEq(
  basic.children(1).map((node) => node.id),
  [2, 3],
  "child order",
);
expectArrayEq(
  basic.child(1).map((node) => node.id),
  [2, 3],
  "child alias",
);
expectArrayEq(
  basic.roots().map((node) => node.id),
  [1, 5],
  "roots",
);
expectArrayEq(
  basic.nodes().map((node) => node.id),
  [1, 2, 3, 4, 5],
  "node order",
);
expectArrayEq(
  basic.items().map((item) => item.id),
  [1, 2, 3, 4, 5],
  "item order",
);
expectEq(
  basic.relation(1, 4),
  data.FlatTreeRelation.Ancestor,
  "ancestor relation",
);
expectEq(
  basic.relation(4, 1),
  data.FlatTreeRelation.Descendant,
  "descendant relation",
);
expectEq(basic.relation(2, 2), data.FlatTreeRelation.Self, "self relation");
expectEq(basic.relation(3, 5), data.FlatTreeRelation.None, "none relation");
debug.assertThrow(basic.isAncestor(1, 4));
debug.assertThrow(!basic.isAncestor(4, 1));
debug.assertThrow(basic.isDescendant(4, 1));
debug.assertThrow(!basic.isDescendant(1, 4));
expectEq(
  basic.render(),
  ".\n├── root\n│   ├── left\n│   │   └── leaf\n│   └── right\n└── orphan\n",
  "basic render",
);
expectEq(
  basic.toString(),
  ".\n├── root\n│   ├── left\n│   │   └── leaf\n│   └── right\n└── orphan\n",
  "toString alias",
);
expectEq(
  basic.render({ maxDepth: 0 }),
  ".\n├── root\n└── orphan\n",
  "max depth root only",
);
expectEq(
  basic.render({ maxDepth: 1 }),
  ".\n├── root\n│   ├── left\n│   └── right\n└── orphan\n",
  "max depth one",
);

const repeated = new data.FlatTree<number, string>([
  { id: 1, content: "root" },
  { id: 2, parentId: 1, content: "dup" },
  { id: 3, parentId: 1, content: "dup" },
  { id: 4, parentId: 2, content: "leaf" },
  { id: 5, parentId: 3, content: "leaf" },
]);

expectEq(
  repeated.render(),
  "root\n├── dup\n│   └── leaf\n└── dup\n    └── leaf\n",
  "repeated subtree render",
);
expectEq(
  repeated.render({ collapseRepeated: false }),
  "root\n├── dup\n│   └── leaf\n└── dup\n    └── leaf\n",
  "expanded repeated subtree render",
);

const objectContent = new data.FlatTree<number, { name: string }>([
  { id: 1, content: { name: "root" } },
]);
expectEq(
  objectContent.render(),
  '{"name":"root"}\n',
  "default object stringify",
);
expectEq(
  objectContent.render({
    stringify: (content) => content.name.toUpperCase(),
  }),
  "ROOT\n",
  "custom stringify",
);

expectThrows(
  () =>
    new data.FlatTree<number, string>([
      { id: 1, content: "a" },
      { id: 1, content: "b" },
    ]),
  "duplicate id check",
);
expectThrows(
  () =>
    new data.FlatTree<number, string>([
      { id: 1, parentId: 2, content: "a" },
      { id: 2, parentId: 1, content: "b" },
    ]),
  "cycle check",
);
expectThrows(() => basic.children(99), "missing child lookup");
expectThrows(() => basic.parent(99), "missing parent lookup");

const incremental = new data.FlatTree<number, string>();
incremental.add({ id: 2, parentId: 1, content: "child" });
incremental.add({ id: 3, parentId: 2, content: "leaf" });
expectArrayEq(
  incremental.roots().map((node) => node.id),
  [2],
  "incremental roots before parent",
);
incremental.add({ id: 1, content: "root" });
expectArrayEq(
  incremental.roots().map((node) => node.id),
  [1],
  "incremental roots after parent",
);
expectEq(
  incremental.render(),
  "root\n└── child\n    └── leaf\n",
  "incremental render",
);
incremental.set({ id: 3, parentId: 1, content: "leaf" });
expectArrayEq(
  incremental.parents(3).map((node) => node.id),
  [2, 1],
  "dag appended parents",
);
expectEq(
  incremental.render(),
  "root\n├── child\n│   └── leaf\n└── leaf\n",
  "dag appended render",
);
incremental.set({ id: 3, parentIds: [1], content: "leaf" });
expectArrayEq(
  incremental.children(1).map((node) => node.id),
  [2, 3],
  "reparented child order",
);
expectEq(
  incremental.render(),
  "root\n├── child\n└── leaf\n",
  "reparented render",
);

const dag = new data.FlatTree<string, string>();
dag.set({ id: "app", content: "app" });
dag.set({ id: "tool", content: "tool" });
dag.set({ id: "main.o", parentId: "app", content: "main.o" });
dag.set({ id: "main.o", parentId: "tool", content: "main.o" });
expectArrayEq(
  dag.parents("main.o").map((node) => node.id),
  ["app", "tool"],
  "multi parent lookup",
);
expectEq(
  dag.relation("app", "main.o"),
  data.FlatTreeRelation.Ancestor,
  "first dag ancestor relation",
);
expectEq(
  dag.relation("tool", "main.o"),
  data.FlatTreeRelation.Ancestor,
  "second dag ancestor relation",
);
expectEq(
  dag.render(),
  ".\n├── app\n│   └── main.o\n└── tool\n    └── main.o\n",
  "dag render",
);

dag.set({ id: "main.o", content: "main object" });
expectArrayEq(
  dag.parents("main.o").map((node) => node.id),
  ["app", "tool"],
  "content-only update keeps parents",
);
