/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.spark.sql;

public enum QEDColumnType {
    DUMMY((byte)0),
    INT((byte)1),
    STRING((byte)2),
    FLOAT((byte)3),
    DATE((byte)4),
    URL_TYPE((byte)5),
    C_CODE((byte)6),
    L_CODE((byte)7),
    LONG((byte)8),
    IP_TYPE((byte)9),
    USER_AGENT_TYPE((byte)10),
    SEARCH_WORD_TYPE((byte)11),
    DUMMY_INT((byte)100),
    DUMMY_FLOAT((byte)101),
    DUMMY_STRING((byte)102);

    private byte _value;

    private QEDColumnType(byte value) {
        this._value = value;
    }

    public byte value() {
        return _value;
    }
}
