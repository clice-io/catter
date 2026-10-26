// Runs while the build holds its connection.
import { connect } from "node:net";

connect(Number(process.env.CATTER_TEST_PORT), "127.0.0.1");
