import { data } from "catter";

function mergeNode<Id extends PropertyKey, Content>(
  tree: data.FlatTree<Id, Content>,
  node: {
    id: Id;
    content: Content;
    parent?: Id[];
    children?: Id[];
  },
): void {
  tree.justMergeNode(node);
}

function size<Id extends PropertyKey, Content>(
  tree: data.FlatTree<Id, Content>,
): number {
  return tree.size();
}

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

const basic = new data.FlatTree<number, string>();
mergeNode(basic, { id: 1, content: "root" });
mergeNode(basic, { id: 2, parent: [1], content: "left" });
mergeNode(basic, { id: 3, parent: [1], content: "right" });
mergeNode(basic, { id: 4, parent: [2], content: "leaf" });
mergeNode(basic, { id: 5, parent: [42], content: "orphan" });

expectEq(size(basic), 5, "basic size");
expectEq(basic.assemble(), true, "basic assemble");
expectArrayEq(basic.roots(), [1], "basic roots");

const basicWalk = basic.walk();
expectEq(basicWalk.first, undefined, "basic walk first");
expectArrayEq(
  basicWalk.children(undefined),
  [1, 5],
  "basic virtual root children",
);
expectArrayEq(basicWalk.children(1), [2, 3], "basic root children");
expectArrayEq(basicWalk.children(2), [4], "basic branch children");
expectArrayEq(basicWalk.children(5), [], "basic orphan children");

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

const incremental = new data.FlatTree<number, string>();
mergeNode(incremental, { id: 2, parent: [1], content: "child" });
mergeNode(incremental, { id: 3, parent: [2], content: "leaf" });

expectEq(incremental.assemble(), true, "incremental assemble before parent");
expectArrayEq(incremental.roots(), [], "incremental roots before parent");

const incrementalBeforeParent = incremental.walk();
expectEq(
  incrementalBeforeParent.first,
  2,
  "incremental walk first before parent",
);
expectArrayEq(
  incrementalBeforeParent.children(undefined),
  [2],
  "incremental virtual root before parent",
);

mergeNode(incremental, { id: 1, content: "root" });

expectArrayEq(incremental.roots(), [1], "incremental roots after parent");

const incrementalAfterParent = incremental.walk();
expectEq(
  incrementalAfterParent.first,
  1,
  "incremental walk first after parent",
);
expectArrayEq(
  incrementalAfterParent.children(1),
  [2],
  "incremental root children after parent",
);
expectArrayEq(
  incrementalAfterParent.children(2),
  [3],
  "incremental child children after parent",
);

mergeNode(incremental, { id: 3, parent: [1], content: "leaf" });

const incrementalDag = incremental.walk();
expectArrayEq(incrementalDag.children(1), [2, 3], "incremental dag children");
expectEq(
  incremental.relation(1, 3),
  data.FlatTreeRelation.Ancestor,
  "incremental dag ancestor relation",
);
expectEq(
  incremental.relation(2, 3),
  data.FlatTreeRelation.Ancestor,
  "incremental original ancestor relation",
);

const dag = new data.FlatTree<string, string>();
mergeNode(dag, { id: "app", content: "app" });
mergeNode(dag, { id: "tool", content: "tool" });
mergeNode(dag, { id: "main.o", parent: ["app"], content: "main.o" });
mergeNode(dag, { id: "main.o", parent: ["tool"], content: "main.o" });

expectEq(dag.assemble(), true, "dag assemble");
expectArrayEq(dag.roots(), ["app", "tool"], "dag roots");

const dagWalk = dag.walk();
expectEq(dagWalk.first, undefined, "dag walk first");
expectArrayEq(dagWalk.children(undefined), ["app", "tool"], "dag top-level");
expectArrayEq(dagWalk.children("app"), ["main.o"], "dag app children");
expectArrayEq(dagWalk.children("tool"), ["main.o"], "dag tool children");
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

const cyclic = new data.FlatTree<number, string>();
mergeNode(cyclic, { id: 1, children: [2], content: "one" });
mergeNode(cyclic, { id: 2, children: [1], content: "two" });
expectEq(cyclic.assemble(), false, "cycle detection");

const mutable = new data.FlatTree<number, string>();
mergeNode(mutable, { id: 1, content: "root" });
mergeNode(mutable, { id: 2, parent: [1], content: "left" });
mergeNode(mutable, { id: 3, content: "right" });
expectEq(mutable.assemble(), true, "mutable assemble");

mutable.update({ id: 2, parent: [3], content: "left moved" });
const mutableWalk = mutable.walk();
expectArrayEq(mutableWalk.children(1), [], "update detaches old parent");
expectArrayEq(mutableWalk.children(3), [2], "update attaches new parent");

mutable.remove(2);
const afterRemove = mutable.walk();
expectArrayEq(afterRemove.children(3), [], "remove detaches stale child edge");
