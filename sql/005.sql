ALTER TABLE plugin DROP FOREIGN KEY plugin_ibfk_1;
ALTER TABLE plugin ADD COLUMN userid INT;


CREATE TABLE events (
       created TIMESTAMP DEFAULT NOW(),
       userid INT,
       plugin_id VARCHAR(128) NOT NULL,
       info TEXT,
       FOREIGN KEY (plugin_id) REFERENCES plugin(id) ON DELETE CASCADE

) ENGINE InnoDB CHARACTER SET utf8 COLLATE utf8_general_ci;
