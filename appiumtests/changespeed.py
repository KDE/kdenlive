#!/usr/bin/env python3

import unittest

import selenium.common.exceptions
from appium import webdriver
from appium.options.common.base import AppiumOptions
from appium.webdriver.common.appiumby import AppiumBy
from selenium.webdriver.support.ui import WebDriverWait


class ChangeSpeedDialogTests(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        options = AppiumOptions()
        options.set_capability("app", "org.kde.kdenlive.desktop")
        options.set_capability("args", "--no-welcome")
        cls.driver = webdriver.Remote(command_executor="http://127.0.0.1:4723", options=options)
        cls.driver.implicitly_wait = 10

    @classmethod
    def tearDownClass(cls):
        cls.driver.quit()

    def wait_for_name(self, text: str, timeout: int = 20):
        wait = WebDriverWait(self.driver, timeout)
        return wait.until(lambda _: self.driver.find_element(by=AppiumBy.NAME, value=text))

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

    def test_change_speed_dialog(self):
        self.wait_for_name("Start Editing").click()

        self.wait_for_name("Add Color Clip…").click()
        self.driver.find_element(
            by=AppiumBy.ACCESSIBILITY_ID,
            value="QApplication.ColorClip_UI.buttonBox.QPushButton",
        ).click()

        self.wait_for_name("Insert Clip Zone in Timeline").click()

        self.click_action(["Change Speed", "Change Speed…"])

        self.wait_for_name("Clip Speed")
        self.wait_for_name("Reverse clip").click()
        self.wait_for_name("OK").click()
        self.wait_not_present("Clip Speed")


if __name__ == "__main__":
    unittest.main()
