#!/usr/bin/env python3

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2016 Microsoft Corporation. All rights reserved.
# SPDX-FileCopyrightText: 2026 Vineet Tiwari

import unittest
from pathlib import Path
from urllib.error import URLError
from urllib.request import urlopen

import selenium.common.exceptions
from appium import webdriver
from appium.options.common.base import AppiumOptions
from appium.webdriver.common.appiumby import AppiumBy
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.support.ui import WebDriverWait


class ChangeSpeedDialogTests(unittest.TestCase):

    SAMPLE_CLIP_NAME = "../tests/dataset/red.mp4"

    @classmethod
    def setUpClass(cls):
        cls.clip_path = cls.ensure_sample_clip()
        caps = {
            "app" : "kdenlive --no-welcome"
        }
        options = AppiumOptions().load_capabilities(caps)
        cls.driver = webdriver.Remote(command_executor="http://127.0.0.1:4723", options=options)
        cls.driver.implicitly_wait = 10

    @classmethod
    def tearDownClass(cls):
        cls.driver.quit()

    @classmethod
    def ensure_sample_clip(cls) -> str:
        clip_path = Path(__file__).resolve().parent / cls.SAMPLE_CLIP_NAME
        print('CHECKING SOURCE CLIP PATH: ' + str(clip_path))
        if clip_path.exists():
            return str(clip_path)

        raise unittest.SkipTest(f"Could not find sample clip")
        return str(clip_path)

    def wait_for_name(self, text: str, timeout: int = 20):
        wait = WebDriverWait(self.driver, timeout)
        try:
            return wait.until(lambda _: self.driver.find_element(by=AppiumBy.NAME, value=text))
        except:
            return None

    def wait_not_present(self, text: str, timeout: int = 20):
        wait = WebDriverWait(self.driver, timeout)

        def _gone(_):
            try:
                self.driver.find_element(by=AppiumBy.NAME, value=text)
                return False
            except selenium.common.exceptions.NoSuchElementException:
                return True

        return wait.until(_gone)

    def click_action(self, name_candidates):
        last_exc = None
        for name in name_candidates:
            try:
                self.driver.find_element(by=AppiumBy.NAME, value=name).click()
                return
            except selenium.common.exceptions.NoSuchElementException as exc:
                last_exc = exc
        if last_exc is not None:
            raise last_exc

    def add_clip_from_file(self, clip_path: str):
        menu = self.driver.find_element(by=AppiumBy.NAME, value="Add Media")
        addAction = menu.find_element(by=AppiumBy.NAME, value="Add Clip or Folder…")
        addAction.click()
        #self.click_action(["Add Clip or Folder…"])

        dialog = self.driver.find_element(by=AppiumBy.ACCESSIBILITY_ID, value="QApplication.QDialog.KFileWidget")
        field = dialog.find_element(by=AppiumBy.ACCESSIBILITY_ID, value="QApplication.QDialog.KFileWidget.QSplitter.QWidget.KUrlComboBox.KLineEdit")
        field.send_keys(str(clip_path))
        dialog.find_element(by=AppiumBy.NAME, value="OK").click()

        # profile match message box - cancel it
        try:
            self.wait_for_name("Warning — Kdenlive", timeout=8)
            dialog = self.driver.find_element(by=AppiumBy.ACCESSIBILITY_ID, value="QApplication.warningYesNo")
            dialog.find_element(by=AppiumBy.NAME, value="Cancel").click()
        except:
            print('Profile dialog not found')

    def test_change_speed_dialog(self):
        self.add_clip_from_file(self.clip_path)

        self.wait_for_name("Insert Clip Zone in Timeline").click()

        # focus timeline
        self.driver.find_element(by=AppiumBy.NAME, value="Switch Monitor").click()
        # select all in timeline
        self.driver.find_element(by=AppiumBy.NAME, value="Select All").click()
        self.click_action(["Change Speed", "Change Speed…"])
        self.wait_for_name("Clip Speed — Kdenlive")
        self.wait_for_name("Reverse clip").click()
        self.wait_for_name("OK").click()
        self.wait_not_present("Clip Speed")


if __name__ == "__main__":
    unittest.main()
