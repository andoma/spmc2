
DROP TABLE tracking;
CREATE TABLE tracking (
       id VARCHAR(64) NOT NULL PRIMARY KEY,
       created TIMESTAMP DEFAULT NOW(),
       updated TIMESTAMP,
       count INT DEFAULT 0,
       ua TEXT NOT NULL,
       ipaddr VARCHAR(32),
       cc VARCHAR(2)
) ENGINE InnoDB CHARACTER SET utf8 COLLATE utf8_general_ci;
